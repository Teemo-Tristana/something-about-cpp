
/*
思路：

shared_ptr智能指针：多个shared_ptr中的T *ptr能指向同一个内存区域（同一个对象），并共同维护同一个引用计数器。

一般来说，智能指针的实现需要以下步骤：
1.一个模板指针T* ptr，指向实际的对象。
2.一个引用次数(必须new出来的，不然会多个shared_ptr里面会有不同的引用次数而导致多次delete)。

3.重载operator*和operator->，使得能像指针一样使用shared_ptr。
4.重载copy constructor，使其引用次数加一。
5.重载operator=，如果原来的shared_ptr已经有对象，则让其引用次数减一并判断引用是否为零(是否调用delete)。
　然后将新的对象引用次数加一。
6.重载析构函数，使引用次数减一并判断引用是否为零(是否调用delete)。


*/
#ifndef SHATED_PTR_HPP
#define SHATED_PTR_HPP

#include <map>
#include <iostream>
using namespace std;

//简单实现版
template <typename T>
class Shared_Ptr
{
private:
    size_t *count; //引用计数
    T *_ptr;

public:
    Shared_Ptr() : count(0), _ptr((T *)0) {}

   // Shared_Ptr(T *p) : count(new size_t(1)), _ptr(p){};
    explicit  Shared_Ptr(T *ptr = nullptr) noexcept : _ptr(ptr)
    {
        if (ptr)
        {
            
            count = new size_t(1);
        }
    }
    //拷贝构造：
    //shared_ptr的拷贝构造： 引用计数+1
    Shared_Ptr(Shared_Ptr<T> &other)
    {
        count = &(++(*other.count));
        _ptr = other._ptr;
    }

    //赋值运算符=：
    //原对象的引用计数-1
    //新对象的引用计数+1
    Shared_Ptr<T> &operator=(Shared_Ptr<T> &other)
    {
        if (this == &other)
            return *this;

        ++(*other.count);                          //此处 other是新的对象
        if (this->_ptr && 0 == (--(*this->count))) //*this是对象
        {
            delete count;
            delete _ptr;
            count = 0;
            _ptr = (T *)0;
            cout << "delete ptr " << endl;
        }

        this->_ptr = other._ptr;
        this->count = other.count;

        return *this;
    }

    //重载 ->
    T *operator->()
    {
        return _ptr;
    }

    //重载 *
    T &operator*()
    {
        return *_ptr;
    }

    ~Shared_Ptr()
    {
        if (_ptr && 0 == (--(*count)))
        {
            delete count;
            delete _ptr;
            count = 0;
            _ptr = (T *)0;
            cout << "delete ptr " << endl;
        }
    }

    size_t getReferenceCount() const
    {
        return *count;
    }
};


//
//参考light-city的shared_ptr实现版
class shared_count
{
private:
    size_t _count;

public:
    shared_count(int c = 1) : _count(c) {}

    void add_count()
    {
        ++_count;
    }

   size_t reduce_count()
    {
        return --_count;
    }

    size_t get_count() const
    {
        return _count;
    }
};

template <typename T>
class myShared_ptr
{
public:
    explicit myShared_ptr(T *ptr = nullptr) noexcept : _ptr(ptr)
    {
        if (ptr)
        {
            _shared_count = new shared_count();
        }
    }

    template <typename U>
    myShared_ptr(const myShared_ptr<U> &other, T *ptr) noexcept
    {
        _ptr = ptr;
        if (_ptr)
        {
            other._shared_count->_shared_count->add_count();
            _shared_count = other._shared_count;
        }
    }

    ~myShared_ptr() noexcept
    {
        if (_ptr && !_shared_count->reduce_count())
        {
            delete _ptr;
            delete _shared_count;
            _ptr = (T *)0;
            _shared_count = 0;
        }
    }

    T &operator*() const noexcept
    {
        return *_ptr;
    }

    T *operator->() const noexcept
    {
        return _ptr;
    }

    operator bool() const noexcept
    {
        return _ptr;
    }

    T *get() const noexcept
    {
        return _ptr;
    }

    // 带模板的拷贝与移动构造函数 模板的各个实例间并不天然就有 friend 关系，因而不能互访私有成员 ptr_ 和 shared_count_。
    // 需要在类内部声明，声明如下
    template <typename U>
    friend class myShared_ptr;

    //调用带模板的复制构造函数
    template <typename U>
    myShared_ptr(const myShared_ptr &other) noexcept
    {
        _ptr = other._ptr;
        if (_ptr)
        {
            other._shared_count->add_count();
            _shared_count = other._shared_count();
        }
    }

    //调用带模板的复制移动函数
    template <typename U>
    myShared_ptr(myShared_ptr<U> &&other) noexcept
    {
        _ptr = other._ptr;
        if (_ptr)
        {
            _shared_count = other._shared_count;
            other._ptr = nullptr;
            other._shared_count = nullptr;
        }
    }

    myShared_ptr &operator=(myShared_ptr rhs) noexcept
    {
        rhs.swap(*this);
        return *this;
    }

    void swap(myShared_ptr &rhs) noexcept
    {
        using std::swap;
        swap(_ptr, rhs._ptr);
        swap(_shared_count, rhs._shared_count);
    }

    size_t use_count() const noexcept
    {
        if (_ptr)
        {
            return _shared_count->get_count();
        }
        else
        {
            return 0;
        }
    }


private:
    T *_ptr;
    shared_count *_shared_count;
};

template <typename T>
void swap(myShared_ptr<T> &lhs, myShared_ptr<T> &rhs) noexcept
{
    lhs.swap(rhs);
}

template <typename T, typename U>
myShared_ptr<T> dynamic_pointer_cast(const myShared_ptr<U> &other) noexcept
{
    T *ptr = dynamic_cast<T *>(other.get());
    return myShared_ptr<T>(other, ptr);
}

template <typename T, typename U>
myShared_ptr<T> static_pointer_cast(const myShared_ptr<U> &other) noexcept
{
    T *ptr = static_cast<T *>(other.get());
    return myShared_ptr<T>(other, ptr);
}

template <typename T, typename U>
myShared_ptr<T> const_pointer_cast(
    const myShared_ptr<U> &other) noexcept
{
    T *ptr = const_cast<T *>(other.get());
    return myShared_ptr<T>(other, ptr);
}

template <typename T, typename U>
myShared_ptr<T> reinterpret_pointer_cast(
    const myShared_ptr<U> &other) noexcept
{
    T *ptr = reinterpret_cast<T *>(other.get());
    return myShared_ptr<T>(other, ptr);
}

#endif