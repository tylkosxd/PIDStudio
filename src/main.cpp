#include "PIDStudio.h"

int main() {
    auto* application = new PIDStudio();
    int result = application->run();
    delete application;
    return result;
}
