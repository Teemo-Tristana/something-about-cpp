
#include "myUnique_ptr.hpp"


int main1() {
   myUnique_ptr<shape> ptr1{create_shape(shape_type::circle)};
//    unique_ptr<shape> ptr2{ptr1}; // error
   myUnique_ptr<shape> ptr2_2{std::move(ptr1)};    // ok
    if (ptr2_2.get() != nullptr && ptr1.get() == nullptr)
        ptr2_2.get()->print();

   myUnique_ptr<shape> ptr3{create_shape(shape_type::rectangle)};
//    ptr1 = ptr3;    // error
    ptr3 = std::move(ptr1); // ok
//    unique_ptr<circle> cl{create_shape(shape_type::circle)};  // error 因为create_shape返回的是shape 不能基类转子类
  myUnique_ptr<circle> cl{new circle()};
  myUnique_ptr<shape> ptr5(std::move(cl));  // ok unique<circle>转unique<circle>
}

int main() {
    myUnique_ptr<int> p1(new int(1));
    cout << "p1 = " << *p1.get() << ", address: " << &p1 <<endl;

    myUnique_ptr<int> p2(std::move(p1));
//    cout << "p1 = " << *p1.get() << "address: " << p1 <<endl;
    cout << "p2 = " << *p2.get() << ",address: " << &p2 <<endl;
}