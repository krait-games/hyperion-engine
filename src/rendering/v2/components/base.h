#ifndef HYPERION_V2_COMPONENTS_BASE_H
#define HYPERION_V2_COMPONENTS_BASE_H

#include "containers.h"
#include <rendering/backend/renderer_instance.h>
#include <hash_code.h>

#include <memory>


namespace hyperion::v2 {

using renderer::Instance;
using renderer::Device;

class Engine;

Device *GetEngineDevice(Engine *engine);

template <class T>
struct Stub {
    Stub() = default;
    ~Stub() = default;

    renderer::Result Create(Engine *)  { return renderer::Result::OK; }
    renderer::Result Destroy(Engine *) { return renderer::Result::OK; }
};

#define STUB_CLASS(name) ::hyperion::v2::Stub<name>

#define ENGINE_COMPONENT_DELEGATE_METHODS \
    inline decltype(m_wrapped) *operator->()             { return &m_wrapped; } \
    inline const decltype(m_wrapped) *operator->() const { return &m_wrapped; }

template <class Type, class ...Args>
struct ID {
    using InnerType = uint32_t;

    explicit constexpr operator InnerType() const { return value; }
    inline constexpr InnerType GetValue() const { return value; }

    inline constexpr bool operator==(const ID &other) const
        { return value == other.value; }

    inline constexpr bool operator<(const ID &other) const
        { return value < other.value; }

    std::tuple<InnerType, ID<Args>...> value;
};

template <class Type>
class EngineComponentBase : public CallbackTrackable<EngineCallbacks> {
protected:
    template <class ObjectType, class InnerType>
    struct IdWrapper {
        using ValueType = InnerType;

        ValueType value{};

        explicit constexpr operator ValueType() const { return value; }
        explicit constexpr operator bool() const { return !!value; }

        inline constexpr ValueType Value() const { return value; }

        inline constexpr bool operator==(const IdWrapper &other) const
            { return value == other.value; }

        inline constexpr bool operator!=(const IdWrapper &other) const
            { return value != other.value; }

        inline constexpr bool operator<(const IdWrapper &other) const
            { return value < other.value; }

        HashCode GetHashCode() const
        {
            HashCode hc;
            hc.Add(value);

            return hc;
        }
    };

public:
    using ID = IdWrapper<Type, uint32_t>;

    static constexpr ID bad_id = ID{0};

    EngineComponentBase()
        : CallbackTrackable(),
          m_init_called(false)
    {
    }

    EngineComponentBase(const EngineComponentBase &other) = delete;
    EngineComponentBase &operator=(const EngineComponentBase &other) = delete;
    ~EngineComponentBase() = default;

    inline ID GetId() const
    {
        if (this == nullptr) {
            DebugLog(
                LogType::Warn,
                "Called GetId() on nullptr\n"
            );

            return bad_id;
        }

        return m_id;
    }

    /* To be called from ObjectHolder<Type> */
    inline void SetId(const ID &id)
        { m_id = id; }

    inline bool IsInit() const { return m_init_called; }

    void Init()
    {
        m_init_called = true;
    }

protected:
    void Destroy()
    {
        m_init_called = false;
    }

    ID m_id;
    bool m_init_called;
};

template <class WrappedType>
class EngineComponent : public EngineComponentBase<WrappedType> {
public:
    template <class ...Args>
    EngineComponent(Args &&... args)
        : EngineComponentBase<WrappedType>(),
          m_wrapped(std::move(args)...),
          m_wrapped_created(false)
    {
#if HYP_DEBUG_MODE
        m_wrapped_destroyed = false;
#endif
    }

    EngineComponent(const EngineComponent &other) = delete;
    EngineComponent &operator=(const EngineComponent &other) = delete;

    ~EngineComponent()
    {
        const char *type_name = typeid(WrappedType).name();

#if 0 // HYP_DEBUG_MODE
        AssertThrowMsg(
            m_wrapped_destroyed,
            "Expected wrapepd object of type %s to have been destroyed before destructor",
            type_name
        );
#endif

        AssertThrowMsg(
            !m_wrapped_created,
            "Expected wrapped object of type %s to be destroyed before destructor, but it was not nullptr",
            type_name
        );
    }

    inline WrappedType &Get() { return m_wrapped; }
    inline const WrappedType &Get() const { return m_wrapped; }

    /* Standard non-specialized initialization function */
    template <class ...Args>
    void Create(Engine *engine, Args &&... args)
    {
        const char *wrapped_type_name = typeid(WrappedType).name();

        AssertThrowMsg(
            !m_wrapped_created,
            "Expected wrapped object of type %s to have not already been created, but it was already created.",
            wrapped_type_name
        );

        auto result = m_wrapped.Create(GetEngineDevice(engine), std::forward<Args>(args)...);
        AssertThrowMsg(
            result,
            "Creation of object of type %s failed.\n\tError Code: %d\n\tMessage: %s",
            wrapped_type_name,
            result.error_code,
            result.message
        );

        m_wrapped_created = true;

#if HYP_DEBUG_MODE
        m_wrapped_destroyed = false;
#endif

        EngineComponentBase<WrappedType>::Init();
    }

    /* Standard non-specialized destruction function */
    template <class ...Args>
    void Destroy(Engine *engine, Args &&... args)
    {
        const char *wrapped_type_name = typeid(WrappedType).name();

#if HYP_DEBUG_MODE
        AssertThrowMsg(
            !m_wrapped_destroyed,
            "Wrapped object of type %s was already destroyed",
            wrapped_type_name
        );
#endif

        AssertThrowMsg(
            m_wrapped_created,
            "Expected wrapped object of type %s to have been created, but it was not yet created (or it was already destroyed)",
            wrapped_type_name
        );

        auto result = m_wrapped.Destroy(GetEngineDevice(engine), std::move(args)...);
        AssertThrowMsg(result, "Destruction of object of type %s failed: %s", wrapped_type_name, result.message);

        m_wrapped_created = false;
        
#if HYP_DEBUG_MODE
        m_wrapped_destroyed = true;
#endif

        EngineComponentBase<WrappedType>::Destroy();
    }

protected:
    using ThisComponent = EngineComponent<WrappedType>;

    WrappedType m_wrapped;
    bool m_wrapped_created;
    bool m_wrapped_destroyed; /* for debug purposes; not really used for anything outside (as objects should be able to be destroyed then recreated) */
};

} // namespace hyperion::v2

#endif // !HYPERION_V2_COMPONENTS_BASE_H

