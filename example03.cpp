#include <iostream>
using namespace std;

//常量指针： const pointer
void const_pointer()
{
    int a = 1;
    int const *p = &a;
    // *p =2; //❌ 常量指针的值不能改
    int b = 100;
    p = &b; //✔
    const int *q = &a;
    // *p = 3; //❌
    q = &b; //✔
    cout << "*p = " << *p << endl;
}

//指针常量
void pointer_const()
{
    int a = 1;
    int *const p = &a;
    *p = 2; // //✔

    int b = 100;
    // p = &b;  //❌
    cout << "*p = " << *p << endl;
}

//常量指针的常指针: 指针p和p指向的内存的数据都不可改
void const_pointer_const()
{
    int a = 1;
    const int *const p = &a;
    // *p = 2;   //❌
    int b = 100;
    // p = &b; //❌
    cout << "*p = " << *p << endl;
}

int main()
{
    const_pointer_const();
    return 0;
}