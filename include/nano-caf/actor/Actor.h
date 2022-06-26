//
// Created by Darwin Yuan on 2022/6/21.
//

#ifndef NANO_CAF_2_404238781FDD4EA28C56C2A4FC1E624B
#define NANO_CAF_2_404238781FDD4EA28C56C2A4FC1E624B

#include <nano-caf/msg/Message.h>
#include <nano-caf/actor/ActorHandle.h>
#include <nano-caf/actor/detail/ExpectMsgHandler.h>
#include <nano-caf/async/detail/CancelTimerObserver.h>
#include <nano-caf/async/Promise.h>
#include <nano-caf/async/Future.h>

namespace nano_caf {
    struct Actor {
        virtual ~Actor() = default;

    protected:
        template<typename T, Message::Category CATEGORY = Message::NORMAL, typename ... ARGS>
        inline auto Send(ActorHandle const& to, ARGS&& ... args) const noexcept -> Status {
            return to.Send<T, CATEGORY>(static_cast<ActorHandle const&>(Self()), std::forward<ARGS>(args)...);
        }

        template<typename T, Message::Category CATEGORY = Message::NORMAL, typename ... ARGS>
        inline auto Reply(ARGS&& ... args) const noexcept -> Status {
            return Send<T, CATEGORY>(CurrentSender(), std::forward<ARGS>(args)...);
        }

        template<typename ATOM, Message::Category CATEGORY = Message::NORMAL, typename R = typename ATOM::Type::ResultType, typename ... ARGS>
        inline auto Request(ActorHandle const& to, ARGS&& ... args) const noexcept -> Future<R> {
            return DoRequest<ATOM, CATEGORY>(to,
                             [](auto&& future) { return Future<R>{future}; },
                             std::forward<ARGS>(args)...);
        }

        template<typename ATOM, Message::Category CATEGORY = Message::NORMAL, typename R = typename ATOM::Type::ResultType, typename Rep, typename Period, typename ... ARGS>
        inline auto Request(ActorHandle const& to, std::chrono::duration<Rep, Period> timeout, ARGS&& ... args) const noexcept -> Future<R> {
            return DoRequest<ATOM, CATEGORY>(to,
                             [&timeout, this](auto& future) {
                                return StartFutureTimer((uint64_t)std::chrono::microseconds(timeout).count(), future);
                             },
                             std::forward<ARGS>(args)...);
        }

        template<typename MSG, typename F, typename R = std::invoke_result_t<F, MSG>>
        auto ExpectMsg(F&& f) noexcept -> Future<R> {
            return DoExpectMsg<MSG>(std::forward<F>(f), [](auto&&) { return Status::OK; });
        }

        template<typename MSG, typename F, typename R = std::invoke_result_t<F, MSG>, typename Rep, typename Period>
        auto ExpectMsg(std::chrono::duration<Rep, Period> timeout, F&& f) noexcept -> Future<R> {
            return DoExpectMsg<MSG>(std::forward<F>(f), [this, &timeout](auto&& handler) {
                return StartExpectMsgTimer((uint64_t)std::chrono::microseconds(timeout).count(), handler);
            });
        }

    private:
        template<typename ATOM, Message::Category CATEGORY = Message::NORMAL, typename R = typename ATOM::Type::ResultType, typename F, typename ... ARGS>
        inline auto DoRequest(ActorHandle const& to, F&& f, ARGS&& ... args) const noexcept -> Future<R> {
            Promise<R> promise;
            auto status = to.DoRequest<typename ATOM::MsgType, CATEGORY>(static_cast<ActorHandle const&>(Self()), promise, std::forward<ARGS>(args)...);
            if(status != Status::OK) {
                return Future<R>{Promise<R>{status}.GetFuture()};
            }
            return f(promise.GetFuture());
        }

        template<typename MSG, typename F, typename R = std::invoke_result_t<F, MSG>, typename CALLBACK>
        auto DoExpectMsg(F&& f, CALLBACK&& callback) noexcept -> Future<R> {
            auto handler = std::make_shared<detail::ExpectMsgHandler<MSG>>();
            if(handler == nullptr) {
                return {};
            }

            auto&& future = handler->GetFuture();
            RegisterExpectOnceHandler(MSG::ID, handler);
            auto status = callback(handler);
            if(status != Status::OK) {
                return {};
            }

            return Future<MSG&>{future}.Then(std::forward<F>(f));
        }

        template<typename R>
        auto StartFutureTimer(TimerSpec const& spec, std::shared_ptr<detail::FutureObject<R>>& f) -> Future<R> {
            using WeakFuturePtr = typename Promise<R>::Object::weak_type;
            WeakFuturePtr weakFuture = f;
            Result<TimerId> result = StartTimer(spec, false,
                   [weakFuture = std::move(weakFuture), weakActor = std::move(Self().ToWeakPtr())]() {
                       ActorPtr actor = weakActor.Lock();
                       if(!actor) return Status::NULL_ACTOR;
                       auto&& future = weakFuture.lock();
                       if(!future) return Status::NULL_PTR;
                       if(!future->OnTimeout()) return;
                       return ActorHandle{std::move(actor)}.Send<FutureDoneNotify>(std::move(future));
                   });
            if(!result.Ok()) {
                return Future<R>{Promise<R>{result.GetStatus()}.GetFuture()};
            }

            f->RegisterObserver(std::make_shared<detail::CancelTimerObserver<R>>(Self().ToWeakPtr(), *result));

            return Future<R>{f};
        }

        template<typename MSG>
        auto StartExpectMsgTimer(TimerSpec const& spec, std::shared_ptr<detail::ExpectMsgHandler<MSG>>& handler) -> Status {
            std::weak_ptr<detail::ExpectMsgHandler<MSG>> weakHandler = handler;
            auto result = StartTimer(spec, false,
                    [this, weakHandler = std::move(weakHandler), weakActor = std::move(Self().ToWeakPtr())]() -> Status {
                        ActorPtr actor = weakActor.Lock();
                        if(!actor) return Status::NULL_ACTOR;
                        auto&& handler = weakHandler.lock();
                        if(!handler) return Status::NULL_PTR;
                        if(!handler->OnTimeout()) return Status::CLOSED;
                        return ActorHandle{std::move(actor)}.Send<TimeoutMsg>([this, weakHandler = std::move(weakHandler)] {
                            auto&& handler = weakHandler.lock();
                            if(!handler) return;
                            handler->Cancel();
                            DeregisterExpectOnceHandler(handler);
                        });
                    });
            if(!result.Ok()) {
                return result.GetStatus();
            }

            handler->GetFuture()->RegisterObserver(std::make_shared<detail::CancelTimerObserver<MSG&>>(Self().ToWeakPtr(), *result));

            return Status::OK;
        }

    protected:
        virtual auto Self() const noexcept -> ActorHandle = 0;
        virtual auto Exit(ExitReason) noexcept -> void = 0;
        virtual auto ChangeBehavior(Behavior const& to) noexcept -> void = 0;

    private:
        virtual auto CurrentSender() const noexcept -> ActorHandle = 0;
        virtual auto RegisterExpectOnceHandler(MsgTypeId, std::shared_ptr<detail::CancellableMsgHandler> const&) noexcept -> void = 0;
        virtual auto DeregisterExpectOnceHandler(std::shared_ptr<detail::CancellableMsgHandler> const&) noexcept -> void = 0;
        virtual auto StartTimer(TimerSpec const& spec, bool periodic, TimeoutCallback&& callback) -> Result<TimerId> = 0;
        virtual auto StopTimer(TimerId timerId) noexcept -> void = 0;
    };
}

#endif //NANO_CAF_2_404238781FDD4EA28C56C2A4FC1E624B
