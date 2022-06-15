//
// Created by Darwin Yuan on 2022/6/15.
//

#ifndef NANO_CAF_2_7CD95F8AA9C54F5BA8D719F757302C47
#define NANO_CAF_2_7CD95F8AA9C54F5BA8D719F757302C47

#include <memory>

namespace nano_caf {

    template <typename ELEM>
    struct Queue {
        Queue() = default;

        auto Enqueue(ELEM* elem) noexcept -> void {
            if(elem == nullptr) return;
            if(m_tail != nullptr) m_tail->m_next = elem;
            else m_head = elem;
            m_tail = elem;
            elem->m_next = nullptr;
        }

        auto Dequeue() noexcept -> ELEM* {
            if(m_head == nullptr) return nullptr;
            auto elem = m_head;
            m_head = elem->m_next;
            if (m_head == nullptr) m_tail = nullptr;
            return elem;
        }

        auto Empty() const noexcept -> bool {
            return m_head == nullptr;
        }

        ~Queue() {
            auto* elem = m_head;
            while(elem != nullptr) {
                std::unique_ptr<ELEM> ptr{elem};
                elem = elem->m_next;
            }
        }

    private:
        ELEM* m_head{};
        ELEM* m_tail{};
    };
}

#endif //NANO_CAF_2_7CD95F8AA9C54F5BA8D719F757302C47
