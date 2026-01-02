RWByteAddressBuffer counter : register(u0);

[shader("compute")]
[numthreads(3, 1, 1)]
void main(uint3 id : SV_DispatchThreadID)
{
    if(id.x > 3)
        return;
    
    counter.Store(id.x * 4, 0);
}