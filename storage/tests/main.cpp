#include "test_framework.hpp"

#include <QCoreApplication>

int main(int argc, char* argv[]) {
    QCoreApplication application(argc, argv);
    return test_framework::run_all();
}
