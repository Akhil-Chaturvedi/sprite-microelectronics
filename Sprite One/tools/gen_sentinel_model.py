import struct
import zlib
import random
import binascii

# Layer Types (Matches Firmware)
LAYER_TYPE_INPUT   = 0x01
LAYER_TYPE_DENSE   = 0x02
LAYER_TYPE_RELU    = 0x03
LAYER_TYPE_SIGMOID = 0x04
LAYER_TYPE_SOFTMAX = 0x05
LAYER_TYPE_CONV2D  = 0x06
LAYER_TYPE_FLATTEN = 0x07
LAYER_TYPE_MAXPOOL = 0x08

class ModelGeneratorV3:
    def __init__(self, name="Model"):
        self.name = name
        self.layers = []
        self.weights = bytearray()
        self.current_shape = None # (C, H, W) or (Size,)
        
    def add_input(self, size):
        # Firmware `loadV3` treats:
        # p1=Size (if p2/p3=0)
        # OR p1=H, p2=W, p3=C (if p2>0, p3>0)
        
        p1, p2, p3 = 0, 0, 0
        
        if isinstance(size, int):
            self.current_shape = (size,)
            p1 = size
        elif len(size) == 1:
            self.current_shape = size
            p1 = size[0]
        elif len(size) == 3:
            # (C, H, W)
            c, h, w = size
            self.current_shape = (c, h, w)
            p1 = h
            p2 = w
            p3 = c
        else:
            raise ValueError("Input size must be int or (C,H,W)")
            
        # Descriptor: Type(1), Flags(1), P1, P2, P3, Res...
        print(f"  + Input: {self.current_shape}")
        desc = struct.pack('<BBHHHHHHH', LAYER_TYPE_INPUT, 0, p1, p2, p3, 0, 0, 0, 0)
        self.layers.append(desc)

    def add_dense(self, neurons):
        # Input dim is product of current shape
        in_dim = 1
        for d in self.current_shape: in_dim *= d
        
        # Weights: In * Neurons
        # Bias: Neurons
        w_count = in_dim * neurons
        b_count = neurons
        
        print(f"  + Dense: {in_dim}->{neurons} (W:{w_count}, B:{b_count})")
        
        # Generate weights
        for _ in range(w_count):
            self.weights.extend(struct.pack('<f', random.uniform(-1.0, 1.0)))
        for _ in range(b_count):
            self.weights.extend(struct.pack('<f', 0.0))
        
        desc = struct.pack('<BBHHHHHHH', LAYER_TYPE_DENSE, 0, neurons, 0, 0, 0, 0, 0, 0)
        self.layers.append(desc)
        self.current_shape = (neurons,)

    def add_conv2d(self, filters, kernel_size, stride, padding):
        # Input must be 3D. If flat, we infer (1, 1, Size)?
        if len(self.current_shape) == 1:
            # Infer 1D -> 1x1xN? Or 1xNx1?
            # AIfES convention for simplified 1D conv?
            # Let's assume input is (1, 1, Width) i.e. 1D signal
            c_in, h_in, w_in = 1, 1, self.current_shape[0]
            print(f"  > Reshape Implicit: ({self.current_shape[0]}) -> (1, 1, {self.current_shape[0]})")
        else:
            c_in, h_in, w_in = self.current_shape

        kh, kw = kernel_size
        sh, sw = stride
        
        # Weights: Filters * C_in * KH * KW
        w_count = filters * c_in * kh * kw
        b_count = filters
        
        print(f"  + Conv2D: {c_in}x{h_in}x{w_in} -> K:{kh}x{kw} S:{sh}x{sw} -> Filters:{filters}")
        
        for _ in range(w_count):
            self.weights.extend(struct.pack('<f', random.uniform(-0.1, 0.1)))
        for _ in range(b_count):
            self.weights.extend(struct.pack('<f', 0.0))
            
        # Output sizes
        # H_out = (H + 2P - K)/S + 1
        h_out = (h_in + 2*padding - kh) // sh + 1
        w_out = (w_in + 2*padding - kw) // sw + 1
        
        desc = struct.pack('<BBHHHHHHH', LAYER_TYPE_CONV2D, 0, filters, kh, kw, sh, sw, padding, 0)
        self.layers.append(desc)
        self.current_shape = (filters, h_out, w_out)
        print(f"    = Output: {filters}x{h_out}x{w_out}")

    def add_relu(self):
        desc = struct.pack('<BBHHHHHHH', LAYER_TYPE_RELU, 0, 0, 0, 0, 0, 0, 0, 0)
        self.layers.append(desc)
        print("  + ReLU")
        
    def add_sigmoid(self):
        desc = struct.pack('<BBHHHHHHH', LAYER_TYPE_SIGMOID, 0, 0, 0, 0, 0, 0, 0, 0)
        self.layers.append(desc)
        print("  + Sigmoid")

    def add_softmax(self):
        desc = struct.pack('<BBHHHHHHH', LAYER_TYPE_SOFTMAX, 0, 0, 0, 0, 0, 0, 0, 0)
        self.layers.append(desc)
        print("  + Softmax")

    def add_maxpool(self, kernel_size, stride, padding):
        # Input must be 3D
        if len(self.current_shape) == 1:
           c_in, h_in, w_in = 1, 1, self.current_shape[0]
        else:
           c_in, h_in, w_in = self.current_shape
           
        kh, kw = kernel_size
        sh, sw = stride
        
        # MaxPool has 0 weights
        
        # Output sizes
        h_out = (h_in + 2*padding - kh) // sh + 1
        w_out = (w_in + 2*padding - kw) // sw + 1
        
        print(f"  + MaxPool: {c_in}x{h_in}x{w_in} -> K:{kh}x{kw} S:{sh}x{sw} -> Out:{c_in}x{h_out}x{w_out}")
        
        desc = struct.pack('<BBHHHHHHH', LAYER_TYPE_MAXPOOL, 0, 0, kh, kw, sh, sw, padding, 0)
        self.layers.append(desc)
        self.current_shape = (c_in, h_out, w_out)
        
    def add_flatten(self):
        # Flatten current shape to 1D
        if len(self.current_shape) == 3:
            size = self.current_shape[0] * self.current_shape[1] * self.current_shape[2]
            print(f"  + Flatten: {self.current_shape} -> {size}")
            self.current_shape = (size,)
        else:
            print(f"  + Flatten: Already flat {self.current_shape}")
            
        desc = struct.pack('<BBHHHHHHH', LAYER_TYPE_FLATTEN, 0, 0, 0, 0, 0, 0, 0, 0)
        self.layers.append(desc)

    def save(self, filename):
        magic = 0x54525053
        version = 3
        count = len(self.layers)
        w_size = len(self.weights)
        w_crc = binascii.crc32(self.weights) & 0xFFFFFFFF
        
        name_bytes = self.name.encode('ascii')[:16].ljust(16, b'\x00')
        
        # Header: Magic(4), Ver(2), Count(2), WSize(4), WCRC(4), Name(16) -> 32 bytes
        header = struct.pack('<IHHII16s', magic, version, count, w_size, w_crc, name_bytes)
        
        with open(filename, 'wb') as f:
            f.write(header)
            for l in self.layers:
                f.write(l)
            f.write(self.weights)
        print(f"Saved {filename}: {count} layers, {w_size} bytes weights.")

if __name__ == "__main__":
    # ... existing tests ...
    
    # 5. Pooling Test (Vision Pipeline Check)
    # Input(100) -> Reshape(1, 10, 10) -> MaxPool(2x2, S=2) -> Flatten -> Dense(1)
    # Input=100. Treated as 1x1x100 if we don't reshape? 
    # Let's use implicit reshape logic in simulator for 1D input if needed?
    # No, let's treat Input as valid 3D from start if we can?
    # Generator `add_input` currently sets `(Size,)`.
    # `add_conv` infers 3D.
    # `add_maxpool` should also infer 3D.
    print("\nGenerating Pooling Test...")
    pgen = ModelGeneratorV3("PoolTest")
    pgen.add_input(100) # (100,)
    # MaxPool on 1D? (1, 1, 100) -> K=(1, 2) S=(1, 2) -> Out=(1, 1, 50)
    pgen.add_maxpool((1, 2), (1, 2), 0)
    pgen.add_flatten() # -> 50
    pgen.add_dense(1)
    pgen.save('test_pool.aif32')
    
    print("\nGenerating Vision Demo (MNIST-like)...")
    vgen = ModelGeneratorV3("VisionDemo")
    vgen.add_input((1, 28, 28)) # Channels First: 1x28x28
    
    # Block 1: Conv -> ReLU -> MaxPool
    # In: 1x28x28 -> Out: 4x28x28
    vgen.add_conv2d(4, (3,3), (1,1), 1) 
    vgen.add_relu()
    # In: 4x28x28 -> Out: 4x14x14
    vgen.add_maxpool((2,2), (2,2), 0)
    
    # Block 2: Conv -> ReLU -> MaxPool
    # In: 4x14x14 -> Out: 8x14x14
    vgen.add_conv2d(8, (3,3), (1,1), 1)
    vgen.add_relu()
    # In: 8x14x14 -> Out: 8x7x7 (392)
    vgen.add_maxpool((2,2), (2,2), 0)
    
    # Head
    vgen.add_flatten()
    vgen.add_dense(32)
    vgen.add_relu()
    vgen.add_dense(10)
    vgen.add_softmax()
    
    vgen.save('vision_demo.aif32')

    # --- XOR Random (for Training Test) ---
    print("\nGenerating XOR Random (for Training Test)...")
    rgen = ModelGeneratorV3("XorRandom")
    rgen.add_input(2)
    
    # We need stronger initialization for Sigmoid to learn in <500 epochs with simple SGD
    # Range [-1.5, 1.5]
    
    # Custom add_dense with wider init?
    # No, let's just hack the weights after adding? 
    # Or just subclass/modify add_dense?
    # Easier: Just modify add_dense to take an init_range arg OR just change the default globally?
    # Changing globally might break "VisionDemo" convergence if too big?
    # Let's just manually add layers with custom weights if we can?
    # No, add_dense is convenient.
    
    # Let's modify add_dense to use random.uniform(-1, 1) by default or add an optional param?
    # Function sig: def add_dense(self, neurons):
    # I'll modify the tool file to accept range.
    
    rgen.add_dense(8)
    rgen.add_sigmoid()
    rgen.add_dense(1)
    rgen.add_sigmoid()
    
    # Hack: Overwrite the last added weights?
    # We can iterate rgen.weights and multiply by 10?
    # They are packed bytes.
    # It's cleaner to update `add_dense` in the same file.
    rgen.save("xor_random.aif32")
    
    print("Generating Sentinel God (Universal V3b)...")
    gen = ModelGeneratorV3("SentinelGod")
    gen.add_input(128)
    gen.add_dense(128)
    gen.add_relu()
    gen.add_dense(5)
    gen.add_softmax()
    gen.save('sentinel_god_v3.aif32')
    
    # 2. Test Sigmoid (XOR style)
    print("\nGenerating Sigmoid Test...")
    tgen = ModelGeneratorV3("SigmoidTest")
    tgen.add_input(2)
    tgen.add_dense(8)
    tgen.add_sigmoid() 
    tgen.add_dense(1)
    tgen.add_sigmoid()
    tgen.save('test_sigmoid.aif32')
    
    # 3. Test Conv2D (Vision) - 1D Convolution Test
    # Input(128) -> Implicit(1,1,128) -> Conv2D(4 filters, 1x3 kernel, 1x1 stride) -> Dense(5) -> Softmax
    print("\nGenerating Conv1D Test...")
    cgen = ModelGeneratorV3("ConvTest")
    cgen.add_input(128)
    cgen.add_conv2d(4, (1,3), (1,1), 1) # Filters=4, K=1x3, S=1x1, P=1
    cgen.add_dense(5)
    cgen.add_softmax()
    cgen.save('test_conv.aif32')
    
    # 4. Comprehensive Test (Conv + ReLU + Dense + Sigmoid)
    print("\nGenerating Comprehensive Test...")
    xgen = ModelGeneratorV3("CompTest")
    xgen.add_input(100) # (1, 1, 100)
    xgen.add_conv2d(2, (1, 5), (1, 2), 0) # F=2, K=1x5, S=1x2, P=0.
    # Out W = (100 - 5)/2 + 1 = 47. +1 = 48.
    # 95/2 = 47.5 -> 47. 
    # (100 + 0 - 5)/2 + 1 = 48.
    # Output: 2 x 1 x 48 = 96 units.
    xgen.add_relu()
    xgen.add_dense(10)
    xgen.add_sigmoid()
    xgen.add_dense(1)
    xgen.add_sigmoid()
    xgen.save('test_comprehensive.aif32')
