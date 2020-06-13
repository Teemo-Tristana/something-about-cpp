#include <iostream>
using namespace std;

struct Student{
    char gender;  //1
    int age;      // 4 
    double score; // 8
}Stu;
//字节对齐前： 1 + 4 + 8 
//字节对齐后： 4 + 4 + 8 = 16 是 最宽类型 8 的倍数

//union 大小是最宽的类型的大小
union PersonInfo
{
    char gender;
    int age;
    double money;
};

//enum 默认用int 尺寸，占 4字节，最大可存储0xffffffff
enum Color{red, green, blue};


int main()
{
    cout << "sizeof(Stu) = " << sizeof(Stu) << endl;
    cout << "sizeof(PersonInfo) = " << sizeof(PersonInfo) << endl;
    cout << "sizeof(Color) = " << sizeof(Color) << endl;
    return 0;
}