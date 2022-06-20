//
// Created by Darwin Yuan on 2022/6/16.
//

#ifndef NANO_CAF_2_D6D87D954DAC419D8DBBF6CD13EC7177
#define NANO_CAF_2_D6D87D954DAC419D8DBBF6CD13EC7177

#include <nano-caf/util/ListElem.h>

using MessageId = uint64_t;

namespace nano_caf {
    struct Message : ListElem<Message>  {
        enum Category : uint64_t {
            NORMAL,
            URGENT
        };

        Message(MessageId type_id,  Category category = Category::NORMAL)
            : m_id(type_id)
            , m_category(category)
        {}

        template<typename BODY>
        auto Body() const noexcept -> BODY const* {
            return nullptr;
        }
        virtual ~Message() = default;

    private:

    public:
        MessageId m_id;
        Category m_category;
    };
}

#endif //NANO_CAF_2_D6D87D954DAC419D8DBBF6CD13EC7177
