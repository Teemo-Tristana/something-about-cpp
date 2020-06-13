#include <iostream>
using namespace std;

//野指针形成原因1：未初始化
void reason_1()
{
    int* p ;
    cout << *p << endl;
}

//野指针形成原因2：free/delete后，没有置空【没有把指针自身释放】
void reason_2()
{
    int *p = nullptr;
    int a = 2;
    p = &a;
    delete p;
    cout << *p << endl;
}

class A{
    public:
        void Func(void){
            cout <<"Func of A" << endl;
        }
};

//野指针形成原因2：指针操作超过作用范围
void reason_3()
{
    A *p;
    {
        A a;
        p = &a; 
    } //a的生命期到此结束
    p->Func(); //超过作用范围

    int* q;
    int b = 1;
    q = &b;
    cout <<*(q+1) << endl;
}


int main()
{
    reason_3();

    return 0;
}