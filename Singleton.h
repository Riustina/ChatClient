#ifndef SINGLETON_H
#define SINGLETON_H

template<typename T>
class Singleton {
public:
    // 删除拷贝构造函数和赋值运算符
    Singleton(const Singleton&) = delete;
    Singleton& operator=(const Singleton&) = delete;

    // 获取单例实例的静态方法（线程安全）
    static T& getInstance() {
        static T instance;  // C++11 保证线程安全
        return instance;
    }

protected:
    Singleton() = default;
    virtual ~Singleton() = default;
};

#endif // SINGLETON_H
