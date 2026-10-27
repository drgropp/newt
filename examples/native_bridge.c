/* Link this C function with native_interop.nt's generated assembly. */
int add_to(int *value, int amount) {
    *value += amount;
    return *value;
}
