RWByteAddressBuffer counter : register(u0);

[numthreads(1, 1, 1)]
void main()
{
    counter.Store(0, 0);
}