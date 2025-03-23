## Memory coordinator Testing  
While designing your memory coordinator, there are a few things to consider:  
1. What are the kinds of memory consumption the coordinator needs to handle?  
2. What is the average case and what are the edge cases?
3. What statistics would help in making a decision to give/take memory?
4. When should you stop reclaiming memory?  
  
The test cases provided in this directory are by no means exhaustive, but are meant to be a sanity check for your memory coordinator. You are encouraged to think about other test cases while designing the coordinator.   
### Test Cases  
We provide three test cases as a starting point. Each test is described below, along with the rationale behind the tests. The setup for each test is the same- there are 4 VMs. All VMs start with 512 MB memory. We assume that the memory for each VM cannot fall below 200 MB. The max memory on each VM can grow up to 2048 MB. More detail on setting up and running the tests is covered in [this](HowToDoTest.md) document.   
#### Test Case 1  
**Scenario**: There are 4 VMs, each with 512 MB. Only VM1 is running a program that consumes memory. Other VMs are idling.   
**Expected coordinator behavior**: VM1 keeps consuming memory until it hits the max limit. The memory is being supplied from the other VMs. Once VM1 hits the max limit, it starts freeing memory.  
**Sample output**:
```
--------------------------------------------------
Memory (VM: aos_vm5)  Actual [656.0], Unused: [251.3984375]
Memory (VM: aos_vm1)  Actual [512.0], Unused: [191.4609375]
Memory (VM: aos_vm4)  Actual [656.0], Unused: [248.828125]
Memory (VM: aos_vm8)  Actual [656.0], Unused: [260.078125]
iter  1
--------------------------------------------------
Memory (VM: aos_vm5)  Actual [512.0], Unused: [94.1953125]
Memory (VM: aos_vm1)  Actual [512.0], Unused: [186.8359375]
Memory (VM: aos_vm4)  Actual [512.0], Unused: [95.2578125]
Memory (VM: aos_vm8)  Actual [512.0], Unused: [105.234375]
iter  2
--------------------------------------------------
...
--------------------------------------------------
Memory (VM: aos_vm5)  Actual [1952.0], Unused: [68.6171875]
Memory (VM: aos_vm1)  Actual [512.0], Unused: [191.35546875]
Memory (VM: aos_vm4)  Actual [656.0], Unused: [248.54296875]
Memory (VM: aos_vm8)  Actual [656.0], Unused: [261.40625]
iter  53
--------------------------------------------------
Memory (VM: aos_vm5)  Actual [1834.0], Unused: [1782.296875]
Memory (VM: aos_vm1)  Actual [512.0], Unused: [191.38671875]
Memory (VM: aos_vm4)  Actual [656.0], Unused: [248.54296875]
Memory (VM: aos_vm8)  Actual [656.0], Unused: [261.40625]
iter  54
--------------------------------------------------
Memory (VM: aos_vm5)  Actual [1673.0], Unused: [1626.5703125]
Memory (VM: aos_vm1)  Actual [512.0], Unused: [191.38671875]
Memory (VM: aos_vm4)  Actual [656.0], Unused: [248.54296875]
Memory (VM: aos_vm8)  Actual [656.0], Unused: [261.40625]
iter  55
--------------------------------------------------
Memory (VM: aos_vm5)  Actual [1517.0], Unused: [1470.48046875]
Memory (VM: aos_vm1)  Actual [512.0], Unused: [191.38671875]
Memory (VM: aos_vm4)  Actual [656.0], Unused: [248.54296875]
Memory (VM: aos_vm8)  Actual [656.0], Unused: [261.4375]
iter  56
--------------------------------------------------
...
```
- The first VM gains memory, which can be eithr supplied by other VMs(e.g iteration 2) or from the host (at iteration 53).   
- Once it reaches the max limit (iteration 53), the program that is consuming memory on the VM stops running, and so the coordinator begins reclaiming memory (iteration 56 and beyond, for example).  

**What are we testing here?**  
This is the base case. The coordinator should be able to allocate/reclaim memory from a single VM.  


#### Test Case 2  
**Scenario**: There are 4 VMs with 512 MB each. All VMs begin consuming memory. They all have similar balloon sizes.  
**Expected coordinator behavior**: The coordinator decides if it can afford to supply more memory to the VMs, or if it should stop doing so. Once the VMs can no longer sustain the memory demands of the program running in them, the program stops running. Now the coordinator should begin reclaiming memory.   
**Sample output**:
```
--------------------------------------------------
Memory (VM: aos_vm5)  Actual [548.0], Unused: [377.609375]
Memory (VM: aos_vm1)  Actual [512.0], Unused: [191.4453125]
Memory (VM: aos_vm4)  Actual [656.0], Unused: [248.44921875]
Memory (VM: aos_vm8)  Actual [656.0], Unused: [261.375]
iter  1
--------------------------------------------------
Memory (VM: aos_vm5)  Actual [519.0], Unused: [360.6484375]
Memory (VM: aos_vm1)  Actual [512.0], Unused: [190.12890625]
Memory (VM: aos_vm4)  Actual [557.0], Unused: [189.58984375]
Memory (VM: aos_vm8)  Actual [569.0], Unused: [223.5859375]
iter  2
--------------------------------------------------
...
--------------------------------------------------
Memory (VM: aos_vm5)  Actual [1984.0], Unused: [113.09765625]
Memory (VM: aos_vm1)  Actual [2040.0], Unused: [65.40234375]
Memory (VM: aos_vm4)  Actual [2040.0], Unused: [93.22265625]
Memory (VM: aos_vm8)  Actual [2040.0], Unused: [63.84765625]
iter  59
--------------------------------------------------
Memory (VM: aos_vm5)  Actual [1984.0], Unused: [83.9140625]
Memory (VM: aos_vm1)  Actual [2040.0], Unused: [65.40234375]
Memory (VM: aos_vm4)  Actual [2040.0], Unused: [64.0703125]
Memory (VM: aos_vm8)  Actual [2040.0], Unused: [74.6796875]
iter  60
--------------------------------------------------
Memory (VM: aos_vm5)  Actual [1984.0], Unused: [80.15234375]
Memory (VM: aos_vm1)  Actual [2040.0], Unused: [1871.14453125]
Memory (VM: aos_vm4)  Actual [2040.0], Unused: [1871.1640625]
Memory (VM: aos_vm8)  Actual [2040.0], Unused: [62.93359375]
iter  61
--------------------------------------------------
Memory (VM: aos_vm5)  Actual [1928.0], Unused: [1758.1875]
Memory (VM: aos_vm1)  Actual [1684.0], Unused: [1515.25]
Memory (VM: aos_vm4)  Actual [1884.0], Unused: [1715.6015625]
Memory (VM: aos_vm8)  Actual [1984.0], Unused: [1816.68359375]
iter  62
...
```
- All VMs start consuming memory, supplied by the host.   
- Their consumption is about the same. Once the coordinator determines that the host can no longer afford to give more memory (iteration 60), it stops supplying more memory.  
- The program on the VMs stops running so the coordinator then starts reclaiming memory from the VMs (iterations 60 and beyond).  

**What are we testing here?**  
When all VMs are consuming memory, we are forcing the coordinator to allocate memory from the host instead.   

#### Test Case 3  
**Scenario**: There are 4 VMs with 512 MB each. VM1 and VM2 initially start consuming memory. After some time, VM1 stops consuming memory but VM2 continues consuming memory. The other 2 VMs are inactive.   
**Expected coordinator behaviour**: The coordinator needs to decide how to supply memory to the VMs with growing demand.  

**Sample output**:  
```
--------------------------------------------------
Memory (VM: aos_vm5)  Actual [456.0], Unused: [61.3359375]
Memory (VM: aos_vm1)  Actual [456.0], Unused: [60.09375]
Memory (VM: aos_vm4)  Actual [256.0], Unused: [73.97265625]
Memory (VM: aos_vm8)  Actual [456.0], Unused: [254.25390625]
iter  1
--------------------------------------------------
Memory (VM: aos_vm5)  Actual [600.0], Unused: [202.9140625]
Memory (VM: aos_vm1)  Actual [600.0], Unused: [201.16796875]
Memory (VM: aos_vm4)  Actual [400.0], Unused: [217.35546875]
Memory (VM: aos_vm8)  Actual [456.0], Unused: [254.25390625]
iter  2
--------------------------------------------------
....
--------------------------------------------------
Memory (VM: aos_vm5)  Actual [1320.0], Unused: [166.90625]
Memory (VM: aos_vm1)  Actual [1320.0], Unused: [163.796875]
Memory (VM: aos_vm4)  Actual [400.0], Unused: [215.6328125]
Memory (VM: aos_vm8)  Actual [456.0], Unused: [254.28515625]
iter  33
--------------------------------------------------
Memory (VM: aos_vm5)  Actual [1320.0], Unused: [1139.0859375]
Memory (VM: aos_vm1)  Actual [1464.0], Unused: [275.08203125]
Memory (VM: aos_vm4)  Actual [400.0], Unused: [215.6328125]
Memory (VM: aos_vm8)  Actual [456.0], Unused: [254.28515625]
iter  34
--------------------------------------------------
...
--------------------------------------------------
Memory (VM: aos_vm5)  Actual [440.0], Unused: [259.4609375]
Memory (VM: aos_vm1)  Actual [2048.0], Unused: [69.13671875]
Memory (VM: aos_vm4)  Actual [400.0], Unused: [215.6328125]
Memory (VM: aos_vm8)  Actual [456.0], Unused: [254.28515625]
iter  39
--------------------------------------------------
Memory (VM: aos_vm5)  Actual [440.0], Unused: [259.4609375]
Memory (VM: aos_vm1)  Actual [2048.0], Unused: [1881.1484375]
Memory (VM: aos_vm4)  Actual [400.0], Unused: [215.6328125]
Memory (VM: aos_vm8)  Actual [456.0], Unused: [254.28515625]
iter  40
...

```
- VM1 and VM2 are consuming memory initially (iteration1, iteration 33).  
- The coordinator supplies them more memory either by taking memory away from VM3 and VM4, or from the host.   
- Then VM1 stops consuming memory, but VM2's demand is still growing (iteration 34).  
- The coordinator can now redirect the memory from VM1 to VM2.  
- Then VM2 reaches its max limit (iteration 39).   
- Now the coordinator starts reclaiming memory from VM2 as well (iteration 40 and beyond).  
 
**What are we testing here?**  
We have a scenario where the memory demands are met by a combination of host memory and using the memory from idling VMs.

## Plotting  trends
You may use the porvided [`plot_graph_memory.py`](./plot_graph_memory.py) to obtain the graphs that will be generated by the autograder. Sample pdf is provided [here](../../res/sample-sol-1.pdf)
