//
// Created by lee on 2019-10-31.
//

# include <iostream>
using namespace std;

extern "C"{
    double f(double);
}

int main() {
    cout << f(1) << endl;
    return 0;
}

