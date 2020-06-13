#ifndef MYUNIQUE_PTR_H
#define MYUNIQUE_PTR_H

#include "shape.h"

template <typename T>
class myUnique_ptr
{
private:
    T *_ptr;

public:
    //所有的智能指针都有一个explicit构造函数：这个构造函数将指针作为参数
    explicit myUnique_ptr(T *ptr = nullptr) noexcept : _ptr(ptr) {}


    // myUnique_ptr(const myUnique_ptr& ) = delete; //禁用复制构造函数  |或者将其放到priavate中
    // myUnique_ptr& operator=(const myUnique_ptr&)= delete; //禁用赋值造函数 |或者将其放到priavate中
    ~myUnique_ptr() noexcept
    {
        delete _ptr;
    }

    //重载 *
    T &operator*() const noexcept
    {
        return *_ptr;
    }

    //重载 ->
    T *operator->() const noexcept
    {
        return _ptr;
    }

    //检查是否有关联的对象
    operator bool() const noexcept
    {
        return _ptr;
    }

    // 返回指向被管理对象的指针 
    T *get() const noexcept
    {
        return _ptr;
    }

    //移动构造函数
    myUnique_ptr(myUnique_ptr &&other) noexcept
    {
        cout << "move ctor" << endl;
        _ptr = other.release(); //交出所有权，保证unqiue_ptr始终之只有一个对象拥有
    }

    //移动构造函数
    template <typename U>
    myUnique_ptr(myUnique_ptr<U> &&other) noexcept
    {
        cout << "U move ctor" << endl;
        _ptr = other.release();
    }

    myUnique_ptr &operator=(myUnique_ptr rhs) noexcept
    {
        rhs.swap(*this);
        return *this;
    }

    //返回一个指向被管理对象的指针，并释放所有权 
    T *release() noexcept
    {
        T *ptr = _ptr;
        _ptr = nullptr;
        return ptr;
    }

    //交换被管理对象 :原来的指针释放所有权
    void swap(myUnique_ptr &rhs) noexcept
    {
        using std::swap;
        swap(_ptr, rhs._ptr);
    }
};

template <typename T>
void swap(myUnique_ptr<T> &lhs, myUnique_ptr<T> &rhs)
{
    lhs.swap(rhs);
}
#endif // !"MYUNIQUE_PTR"
