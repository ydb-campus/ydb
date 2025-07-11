#pragma once
#include <format>
#include <iostream>
#include <ostream>
#include <string_view>
#include <utility>

namespace pmerge {
    struct noop_ostr {
        void flush() {
        }
    };
    template <typename T>
    noop_ostr& operator<<(noop_ostr& ostr, const T&) {
        return ostr;
    }

    inline noop_ostr& operator<<(noop_ostr& __os,
                                 [[maybe_unused]] std::ostream& (*f)(std::ostream&)) {
        return __os;
    }
    namespace detail {
        class Output {
        public:
            void Mute() {
                muted = true;
            }
            void Unmute() {
                muted = false;
            }

            friend Output& operator<<(Output& self, std::ostream& (*f)(std::ostream&));
            template <typename T>
            friend Output& operator<<(Output& self, const T& val);

        private:
            noop_ostr noop_;
            bool muted = false;
        };
        template <typename T>
        Output& operator<<(Output& ostr, [[maybe_unused]]  const T& val) {
#ifndef NDEBUG
            if (!ostr.muted) {
                std::cout << val;
            }
#endif
            return ostr;
        }

        inline Output& operator<<(Output& ostr, [[maybe_unused]]  std::ostream& (*f)(std::ostream&)) {
#ifndef NDEBUG
            if (!ostr.muted) {
                std::cout << f;
            }
#endif
            return ostr;
        }
    } // namespace detail
    inline auto output = detail::Output{};

    template <typename... Args>
    inline void println([[maybe_unused]]  std::format_string<Args...> fmt, [[maybe_unused]]  Args&&... args) {
#ifndef NDEBUG
        output << std::format(fmt, std::forward<Args>(args)...) << std::endl;
#endif
    }
} // namespace pmerge

namespace pmerge::utils {
    inline void PrintIfDebug([[maybe_unused]]  std::string_view string) {
#ifndef NDEBUG
        pmerge::output << string;
        pmerge::output << std::endl;
#endif
    }
} // namespace pmerge::utils
