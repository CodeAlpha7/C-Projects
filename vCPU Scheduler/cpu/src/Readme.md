## Project Instruction Updates:

1. Complete the function CPUScheduler() in vcpu_scheduler.c
2. If you are adding extra files, make sure to modify Makefile accordingly.
3. Compile the code using the command `make all`
4. You can run the code by `./vcpu_scheduler <interval>`
5. While submitting, write your algorithm and logic in this Readme.




## vCPU scheduler Implementation Logic Workflow:

1. Make connection to hypervisor and check if number of arguments is corret.
2. Get host pCPU info - number of pCPUs
3. Initialize an array (dynamic) to store each pCPU's usage percentage
4. Get vCPU stats
	a. initialize various variables and arrays
	- currentStats & previousStats = arrays to store active VM stats
	- timeDuration = time interval for each iteration in nanoseconds*10^9
	- previousDomains = variable to track number of active VMs in prev iter
	- iter = iteration counter

	b. Continuous Loop to periodically (defined) monitor the system.
	- domainList = retrieves list of active VMs
	- numberOfDomains = gets the number of active VMs during that iteration. if nothing, break loop.

	c. Now, to ensure stability of algorithm, we have make sure we have 2 iterations with the same number of VMs. For this compare the numbers of current with previous. Only if same calculate CPU metrics and perform pinning.

	d. Reset each PCPUs array to ZERO for next iteration
	e. Sleep for specified time interval before restarting.

To get vCPU stats:
	a. Declare variables for storing vCPU info - info, mapsize, map.
	b. Initialize statistics for each active VM
	c. Copy info to currentStats (used in main)

5. Now, we need to find the most and least utilized pCPUs based on their usage percentages. Refer load balancing logic.


### ALGORITHM STARTS HERE:

6. The program performs load balancing by redistributing vCPUs among pCPUs. It identifies the pCPU with the highest utilization and the pCPU with the lowest utilization. To identify utilization of pCPU, just add all the individual utilizations of vCPUs
 - first, identify the busiest pCPU. Done the simple way. Initialize low_pointer with MAX value and high_pointer with MIN value. Iterate all pCPUs to constantly compare these variables and assign until respective pCPUs are found.
 - then check if load balancing is needed. (Refer Logic Below)

 - calculate the highest utilization vCPU from busiest pCPU by subtracting current CPU time with CPU time in the previous interval divided by the time interval. multiplying by 100 gives the percentage. We will migrate/pin this vCPU to least busy pCPU to balance load.

 - This process is repeated as needed until load balancing criteria are met.


#### LOAD BALANCING LOGIC:

A constant threshold of 20 has been fixed which decides whether load balancing is should be done or not. If difference between maximum and minimum utilizations in "find" function is less than 20, it means that the pCPUs are reasonably balanced and there is no need for load balancing, else there is. Justification for using 20 particularly lies in 8 VMs being used for testing. Since the maxmimum usage we can have is 100, load equally divided between all 8 VMs means the difference should not be more than one VMs worth.


