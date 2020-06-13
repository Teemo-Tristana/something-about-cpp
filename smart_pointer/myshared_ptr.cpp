#include "myshared_ptr.hpp"

int main()
{
    Shared_Ptr<string> p1(new string("a=hello"));
    cout << "p1 = " << *p1 << ", RefCount = " << p1.getReferenceCount() << endl;

    Shared_Ptr<string> p2(p1);
    cout << "p1 = " << *p1 << ", RefCount = " << p1.getReferenceCount() << endl;
    cout << "p2 = " << *p2 << ", RefCount = " << p2.getReferenceCount() << endl;

    Shared_Ptr<string> p3(new string("world"));
    cout << "p3 = " << *p3 << ", RefCount = " << p3.getReferenceCount() << endl;

    // p2 = p3;
    p3 = p2;
    cout << "p1 = " << *p1 << ", RefCount = " << p1.getReferenceCount() << endl;
    cout << "p2 = " << *p2 << ", RefCount = " << p2.getReferenceCount() << endl;
    cout << "p3 = " << *p3 << ", RefCount = " << p3.getReferenceCount() << endl;

    return 0;
}

int test()
{
    myShared_ptr<string> p1(new string("a=hello"));
    cout << "p1 = " << *p1 << ", RefCount = " << p1.use_count() << endl;

    myShared_ptr<string> p2(p1);
    cout << "p1 = " << *p1 << ", RefCount = " << p1.use_count() << endl;
    cout << "p2 = " << *p2 << ", RefCount = " << p2.use_count() << endl;

    myShared_ptr<string> p3(new string("world"));
    cout << "p3 = " << *p3 << ", RefCount = " << p3.use_count() << endl;

    // p2 = p3;
    p3 = p2;
    cout << "p1 = " << *p1 << ", RefCount = " << p1.use_count() << endl;
    cout << "p2 = " << *p2 << ", RefCount = " << p2.use_count() << endl;
    cout << "p3 = " << *p3 << ", RefCount = " << p3.use_count() << endl;

    return 0;
}