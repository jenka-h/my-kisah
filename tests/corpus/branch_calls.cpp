__attribute__((noinline)) int positive() {
    return 1;
}

__attribute__((noinline)) int non_positive() {
    return 2;
}

int choose(int x) {
    if (x > 0)
        return positive();

    return non_positive();
}

int main() {
    return choose(3);
}
