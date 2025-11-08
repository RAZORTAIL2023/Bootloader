#include <memory_resource>

class monotonic_buffer_resource_ex : public std::pmr::memory_resource {
private:
    std::pmr::monotonic_buffer_resource* proxy;
    size_t val_used = 0;
    size_t val_allocationCount=0;
    size_t val_peak = 0;
    size_t capacity = 0;

public:
    explicit monotonic_buffer_resource_ex(std::pmr::monotonic_buffer_resource* proxy, size_t capacity) : proxy(proxy), capacity(capacity) {}

protected:
    void* do_allocate(size_t __bytes, size_t __alignment) override {
        void* ptr = proxy->allocate(__bytes, __alignment);

        val_used += __bytes;
        val_allocationCount++;
        val_peak = __bytes > val_peak ? __bytes : val_peak;
        return ptr;
    }

    void do_deallocate(void* p, size_t __bytes, size_t __alignment) override {
        proxy->deallocate(p, __bytes, __alignment);
    }

    bool do_is_equal(const std::pmr::memory_resource& __other) const noexcept override {
        return this == std::addressof(__other);
    }

public:
    size_t used() const { return val_used; }
    size_t allocationCount() const { return val_allocationCount; }
    size_t peak() const { return val_peak; }
    size_t remain() const { return capacity - val_used; }

    void release() {
        proxy->release();
        val_used = 0;
        val_allocationCount = 0;
        val_peak = 0;
    }
};