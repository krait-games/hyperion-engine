#ifndef HYPERION_DEFINES_H
#define HYPERION_DEFINES_H

#define HYP_VULKAN 1

// stl helpers and such

#define HYP_STR(x) #x
#define HYP_METHOD(method) HYP_STR(method)

#define HYP_DEF_STRUCT_COMPARE_EQL(hyp_class) \
    bool operator==(const hyp_class &other) const { \
        return !std::memcmp(this, &other, sizeof(*this)); \
    }

#define HYP_DEF_STL_HASH(hyp_class) \
    template<> \
    struct std::hash<hyp_class> { \
        size_t operator()(const hyp_class &obj) const \
        { \
            return obj.GetHashCode().Value(); \
        } \
    }

#define HYP_DEF_STL_ITERATOR(container) \
    [[nodiscard]] Iterator Begin()             { return container.begin(); }  \
    [[nodiscard]] Iterator End()               { return container.end(); }    \
    [[nodiscard]] ConstIterator Begin() const  { return container.begin(); }  \
    [[nodiscard]] ConstIterator End() const    { return container.end(); }    \
    [[nodiscard]] Iterator begin()             { return container.begin(); }  \
    [[nodiscard]] Iterator end()               { return container.end(); }    \
    [[nodiscard]] ConstIterator begin() const  { return container.begin(); }  \
    [[nodiscard]] ConstIterator end() const    { return container.end(); }    \
    [[nodiscard]] ConstIterator cbegin() const { return container.cbegin(); } \
    [[nodiscard]] ConstIterator cend() const   { return container.cend(); }

#define HYP_DEF_STL_BEGIN_END(_begin, _end) \
    [[nodiscard]] Iterator Begin()             { return _begin; } \
    [[nodiscard]] Iterator End()               { return _end; }   \
    [[nodiscard]] ConstIterator Begin() const  { return _begin; } \
    [[nodiscard]] ConstIterator End() const    { return _end; }   \
    [[nodiscard]] Iterator begin()             { return _begin; } \
    [[nodiscard]] Iterator end()               { return _end; }   \
    [[nodiscard]] ConstIterator begin() const  { return _begin; } \
    [[nodiscard]] ConstIterator end() const    { return _end; }

#define HYP_ENABLE_IF(cond, return_type) \
    typename std::enable_if_t<cond, return_type>

// switches

#if defined(HYPERION_BUILD_RELEASE_FINAL) && HYPERION_BUILD_RELEASE_FINAL
    #define HYPERION_BUILD_RELEASE 1 // just to ensure
#endif

#if defined(__GNUC__) || defined(__GNUG__) || defined(__clang__)
    #define HYP_CLANG_OR_GCC 1
#elif defined(_MSC_VER)
    #define HYP_MSVC 1
#else
    #error Unknown compiler
#endif

#if defined(HYP_CLANG_OR_GCC) && HYP_CLANG_OR_GCC
    #define HYP_PACK_BEGIN __attribute__((__packed__))
    #define HYP_PACK_END
    #define HYP_FORCE_INLINE __attribute__((always_inline))
#endif

#if defined(HYP_MSVC) && HYP_MSVC
    #define HYP_PACK_BEGIN __pragma(pack(push, 1))
    #define HYP_PACK_END __pragma(pack(pop))
    #define HYP_FORCE_INLINE __forceinline
#endif

#if defined(_WIN32) && _WIN32
    #define HYP_WINDOWS 1

    #define HYP_FILESYSTEM_SEPARATOR "\\"
#else
    #define HYP_FILESYSTEM_SEPARATOR "/"
#endif

#define HYP_USE_EXCEPTIONS     0

#if !defined(HYPERION_BUILD_RELEASE) || !HYPERION_BUILD_RELEASE
    #define HYP_DEBUG_MODE 1
#endif

#if defined(HYPERION_BUILD_RELEASE_FINAL) && HYPERION_BUILD_RELEASE_FINAL
    #define HYP_ENABLE_BREAKPOINTS 0
#else
    #define HYP_ENABLE_BREAKPOINTS 1
#endif

#if HYP_CLANG_OR_GCC
    #define HYP_DEBUG_FUNC_SHORT __FUNCTION__
    #define HYP_DEBUG_FUNC       __PRETTY_FUNCTION__
    #define HYP_DEBUG_LINE       (__LINE__)

    #if HYP_ENABLE_BREAKPOINTS
        #define HYP_BREAKPOINT       (__builtin_trap())
    #endif
#elif HYP_MSVC
    #define HYP_DEBUG_FUNC_SHORT (__FUNCTION__)
    #define HYP_DEBUG_FUNC       (__FUNCSIG__)
    #define HYP_DEBUG_LINE       (__LINE__)
    #if HYP_ENABLE_BREAKPOINTS
        #define HYP_BREAKPOINT       (__debugbreak())
    #endif
#else
    #define HYP_DEBUG_FUNC_SHORT ""
    #define HYP_DEBUG_FUNC ""
    #define HYP_DEBUG_LINE (0)
    #if HYP_ENABLE_BREAKPOINTS
        #define HYP_BREAKPOINT       (void(0))
    #endif
#endif

#if HYP_DEBUG_MODE
    #define HYP_BREAKPOINT_DEBUG_MODE HYP_BREAKPOINT
#else
    #define HYP_BREAK_IF_DEBUG_MODE  (void(0))
#endif

#if !HYP_ENABLE_BREAKPOINTS
    #define HYP_BREAKPOINT           (void(0))
#endif

#if defined(HYP_USE_EXCEPTIONS) && HYP_USE_EXCEPTIONS
    #define HYP_THROW(msg) throw ::std::runtime_error(msg)
#else
    #if HYP_DEBUG_MODE
        #define HYP_THROW(msg) \
            do { \
                HYP_BREAKPOINT; \
                std::terminate(); \
            } while (0)
    #else
        #define HYP_THROW(msg) std::terminate()
    #endif
#endif

#endif // !HYPERION_DEFINES_H