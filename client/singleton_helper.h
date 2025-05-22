#ifndef SINGLETON_HELPER_H
#define SINGLETON_HELPER_H

template <class T>
class Singleton {
public:
    static T& instance() {
        static T instance;
        return instance;
    }

protected:
    Singleton() {}
    virtual ~Singleton() {}

private:
    Singleton(const Singleton&) = delete;
    Singleton& operator=(const Singleton&) = delete;
};

#endif // SINGLETON_HELPER_H
