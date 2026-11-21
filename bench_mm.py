"""Tensor matmul TFLOPS on the 3060 (same 8192x8192 test as newpc's GPU_UNLOCK_NEWPC.md).
Stock 3060: FP16/BF16 ~26-27, TF32 ~13. Unlocked: ~47-49 / ~24."""
import time

import torch

n = 8192
torch.backends.cuda.matmul.allow_tf32 = True
print(torch.cuda.get_device_name(0))
for name, dt in (('FP16', torch.float16), ('BF16', torch.bfloat16), ('TF32', torch.float32)):
    a = torch.randn(n, n, device='cuda', dtype=dt)
    b = torch.randn(n, n, device='cuda', dtype=dt)
    for _ in range(5):
        a @ b
    torch.cuda.synchronize()
    t = time.time()
    k = 30
    for _ in range(k):
        a @ b
    torch.cuda.synchronize()
    print('%s %.1f TFLOPS' % (name, 2 * n ** 3 * k / (time.time() - t) / 1e12))
