#include "header.h"



char *strcpy(char *dst, const char *src)
{
    assert(dst != nullptr);
    assert(src != nullptr);
    char *ret = dst;
    memcpy(dst, src, strlen(src + 1));
    return ret;
}

/*
memove和memcpy作用相同，都是从拷贝一定长度的内存内容
唯一区别在于: 当内存发生重叠时，memmove保证拷贝结果是准确度，而memcpy不保证拷贝结果是正确的。
*/

void * memcpy(void *dst, const void *src, unsigned int count)
{
    assert(dst != nullptr);
    assert(src != nullptr);
    char *pd = (char *)dst;
    const char *ps = (const char *)src;
    bool flag1 = (pd >= src && pd < ps + count);
    bool flag2 = (ps >= pd && ps < pd + count);

    if (!flag1 || flag2)
    {
        cout << "overlap" << endl;
        return nullptr;
    }
    while (count--)
    {
        *pd = *ps;
        pd++;
        ps++;
    }
    return dst;
}

void *memmove(void *dst, const void *src, size_t count)
{
    assert(dst != nullptr);
    assert(src != nullptr);
    char *pd = (char *)dst;
    const char *ps = (const char *)src;

    //若不重叠或重叠第一种情况下，则从前完后拷贝
    if (pd <= ps || pd >= (ps + count))
    {
        while (count--)
        {
            *pd = *ps;
            pd++;
            ps++;
        }
    }
    else  // 内存重叠，从后往前拷贝
    {
        pd = (char *)dst + count - 1;
        ps = (char *)src + count - 1;
        while (count--)
        {
            *pd = *ps;
            pd--;
            ps--;
        }
    }
    return dst;
}

