# Cache一致性协议

Cache一致性协议（Cache Coherence Protocol）是用于确保在多处理器系统中，各个处理器的缓存中的数据是一致的。以下是一些常见的缓存一致性协议：

## 1. MSI协议
MSI协议将缓存块的状态分为三种：
- **Modified (M)**: 缓存块已被修改，且与主存中的数据不一致。
- **Shared (S)**: 缓存块未被修改，且可能存在于其他缓存中。
- **Invalid (I)**: 缓存块无效。

## 2. MESI协议
MESI协议在MSI协议的基础上增加了一个状态：
- **Exclusive (E)**: 缓存块未被修改，且仅存在于当前缓存中。

## 3. MOESI协议
MOESI协议在MESI协议的基础上增加了一个状态：
- **Owner (O)**: 缓存块已被修改，但其他缓存中也存在该块，且当前缓存负责将数据写回主存。

## 4. MESIF协议
MESIF协议在MESI协议的基础上增加了一个状态：
- **Forward (F)**: 缓存块未被修改，且当前缓存负责响应其他处理器的读取请求。

## 5. Dragon协议
Dragon协议是一种更新（Update）协议，与前面提到的无效（Invalidate）协议不同。它通过广播更新来保持缓存一致性。

## 6. Firefly协议
Firefly协议是一种混合协议，结合了无效和更新协议的优点。

这些协议通过不同的机制和状态转换来确保多处理器系统中的缓存一致性，从而提高系统的性能和可靠性。

缓存一致性协议在多处理器系统中运行时，主要通过以下机制来确保各个处理器的缓存中的数据是一致的：

状态转换：每个缓存块都有一个状态，这些状态会根据缓存块的操作（如读取、写入）以及其他处理器的操作进行转换。例如，在MESI协议中，缓存块可以在Modified、Exclusive、Shared和Invalid状态之间转换。

总线嗅探：处理器通过总线嗅探（Bus Snooping）来监控其他处理器对内存的操作。当一个处理器对某个地址进行操作时，其他处理器会检查自己的缓存是否包含该地址的数据，并根据需要更新或无效化缓存块。

广播更新：在一些协议（如Dragon协议）中，当一个处理器修改了某个缓存块的数据时，会将更新广播给其他处理器，以确保它们的缓存中也有最新的数据。

写回和写直达：写回（Write-back）和写直达（Write-through）是两种常见的写策略。写回策略下，数据修改只在缓存中进行，只有当缓存块被替换时才写回主存。而写直达策略下，每次数据修改都会立即写回主存。

具体来说，以MESI协议为例，当两个处理器核对同一个地址的数据进行Load、Add、Store操作时，以下是可能的步骤：

Load操作：处理器A和处理器B都从主存中加载数据到各自的缓存中，缓存块状态为Shared（S）。

Add操作：假设处理器A对数据进行加法操作，缓存块状态变为Modified（M），并通过总线嗅探通知处理器B将其缓存块状态设为Invalid（I）。

Store操作：处理器A将修改后的数据存回缓存中，缓存块状态仍为Modified（M）。如果处理器B需要再次访问该数据，会通过总线嗅探从处理器A的缓存中获取最新数据。

通过这些机制，缓存一致性协议确保了多处理器系统中的数据一致性，从而保证了最终结果的正确性。

在cache块中用额外的数据位标记其状态Modified Exclusive Shared Invalid
初始状态为Invalid，假设处理器为A，B

Load操作：处理器A和处理器B都从主存中加载数据到各自的缓存中，缓存块状态从Invalid变为Shared

我们假设处理器A更先完成加法操作
Add操作：假设处理器A对数据进行加法操作将缓存块变为Modified操作，并通过总线嗅探通知处理器B将其缓存块状态设为Invalid，如果处理器B需要进行加法操作访问该数据，会通过总线嗅探从处理器A的缓存中获取最新数据。

Store操作：处理器A将修改后的数据存回缓存中，缓存块状态仍为Modified，如果处理器B需要再次访问该数据，会通过总线嗅探从处理器A的缓存中获取最新数据。


//describe the transformation of directory-based protocol
//need an edge <Exclusive,Share>
dir[addr].stage
dir[addr].Exclusive_for_whichproc

int main(){
    loop:
      cores ops sequence input
      // format : (opcode, addr, data), (), ... 
      for eache op in sequence:
        switch(dir[])
        if(ask_dir_stage(addr) == I):
            if(opcode == Load)://read miss
                dir[addr].stage <- S
            end if
            if(opcode == Store)://write miss
                dir[addr].stage <- E
            end if

        end if

        if(cache[addr].stage == Exclusive):
            if(opcode == Store && dir[addr].stage == E 
              && dir[addr].Exclusive_for_whichproc != this_proc):// other proc write miss
                dir[addr].stage <- S
            end if
        end if
}

tpy