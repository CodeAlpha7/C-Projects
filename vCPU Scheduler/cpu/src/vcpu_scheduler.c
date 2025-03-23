#include<stdio.h>
#include<stdlib.h>
#include<libvirt/libvirt.h>
#include<math.h>
#include<string.h>
#include<unistd.h>
#include<limits.h>
#include<signal.h>
#define MIN(a,b) ((a)<(b)?a:b)
#define MAX(a,b) ((a)>(b)?a:b)

int is_exit = 0; // DO NOT MODIFY THIS VARIABLE

//used to determine if load-balancing between pCPUs is required. Reason = 8 VMs
static const double THRESHOLD = 20.0;

//here, domain refers to VM
typedef struct DomainStats {
    virDomainPtr domain; //pointer to VM
    int VCPUnumber;
    unsigned long long *vcpuTimeArr; //array to store vCPU time in nanoseconds
    int *pcpuArr; //stores mapping of vCPUs to pCPUs
} DomainStats;


//Initializes stats for all active VMs (domains)
//Retreieves vCPU information including vCPU times and pCPU mappings for each VM
void initializeDomainStats(int PCPUnumber, virDomainPtr* domainList, DomainStats* currentStats, int numberOfDomains){

    //declaring variables for storing vCPU information
	virVcpuInfoPtr vcpuInfo;
    size_t cpuMapSize;
    unsigned char *cpuMaps;
	
	//loop to initialize statistics for each active VM
    for(int i=0;i<numberOfDomains; i++){
		//queries only a running domain and gets the number of vCPUs for current domain
        int VCPUnumber = virDomainGetVcpusFlags(domainList[i], VIR_DOMAIN_AFFECT_LIVE);
        if(VCPUnumber == -1){
            printf("VM is NOT ACTIVE!\n");
            exit(1);
        }
		//allocating memory for arrays to store vCPU times and pCPU mappings.
        currentStats[i].vcpuTimeArr = (unsigned long long *)calloc(VCPUnumber, sizeof(unsigned long long));
        currentStats[i].pcpuArr = (int *)calloc(VCPUnumber, sizeof(int));
		//allocating memory to store vCPU information
		// - vCPU number, vCPU state, CPUtime in nanosec, real CPU number
        vcpuInfo = (virVcpuInfoPtr)calloc(VCPUnumber, sizeof(virVcpuInfo));

		//calculate size of CPU map based on the number of physical CPUs
        cpuMapSize = VIR_CPU_MAPLEN(PCPUnumber);
        cpuMaps = (unsigned char*)calloc(VCPUnumber, cpuMapSize);
 

        //Retrieving info about vCPUs for current domain
		//Populates both vcpuinfo and cpuMaps
		int temp = virDomainGetVcpus(domainList[i], vcpuInfo, VCPUnumber, cpuMaps, cpuMapSize);
        if(temp == -1){
            printf("No info. No vCPUs\n");
            exit(1);
        }

		//Now, copy all this vCPU information to currentStats, send back to main and use to calculate CPUtime.
		// This is only for one VM. Loop on i for each active VM.
        int j=0;
        while(j < VCPUnumber){
            currentStats[i].vcpuTimeArr[j] = vcpuInfo[j].cpuTime;
            currentStats[i].pcpuArr[j] = vcpuInfo[j].cpu;
            j++;
        }
        currentStats[i].VCPUnumber = VCPUnumber;
        currentStats[i].domain = domainList[i];
    }
    return;
}
//This function is responsible for pinning vCPUs to pCPUs to maintain affinity.
void PinAffinity(int PCPUnumber, int numberOfDomains, DomainStats *currentStats){
    size_t cpuMapSize = VIR_CPU_MAPLEN(PCPUnumber);
    unsigned char cpuMap = 0x1;
    for(int i=0;i<numberOfDomains;i++){
		//nested loop to iterate through all vCPUs of the current domain.
        for(int j=0; j<currentStats[i].VCPUnumber; j++){
            int realCPU = currentStats[i].pcpuArr[j];
            cpuMap = 0x1 << realCPU; //physical CPU (realCPU) to which the current vCPU is mapped.
            virDomainPinVcpu(currentStats[i].domain, j, &cpuMap, cpuMapSize);
			//Uses virDomainPinVcpu to pin the vCPU (j) of the current domain to the specified pCPU (realCPU) using the cpuMap.
        }
    }
}

//performs load balancing by re-pinning vCPUs based on CPU usage metrics.
void pinVcpu(double *UsagePCPU, int numberPCPU, DomainStats *currentStats, DomainStats *previousStats, int numberOfDomains, double timeDuration){
    //variables to store most and least utilized pCPUs
	int maxUtilCpu, minUtilCpu;

    printUsage(UsagePCPU, numberPCPU);
    // Find pCPUs which are most and least utilized and stores in above variables
    int result = find(UsagePCPU, numberPCPU, &maxUtilCpu, &minUtilCpu);
    printf("Lightest pCPU: %d, Heaviest pCPU: %d\n", minUtilCpu, maxUtilCpu);

    // If no balancing is required, it prints a message and performs 
	//affinity pinning to maintain the current pinning.
	if(result == 0){
        printf("\t-EQUALLY BALANCED. STANDING DOWN.\n");
        PinAffinity(numberPCPU, numberOfDomains, currentStats);
        return;
    }

    // Take Lightest vCPU from maxUtilCPU and assign it minUtilCpu
    size_t cpuMapSize = VIR_CPU_MAPLEN(nPCPU);
    unsigned char minUtilCpuMap = 0x1 << minUtilCpu;
    //same logic as used in find. Set lowest_variable to highest number 
    //and keep assigning value through iteration until it is actually the lowest.
    double leastweightVCPU = DBL_MAX;
    virDomainPtr leastWeightCPUdomain;
    int leastWeightCPUnumber;

    //iterate all domains
    for(int i=0;i<numberOfDomains;i++){
        //iterate all vCPUs in that domain (to find the lowest vCPU)
        for(int j=0; j<currentStats[i].numberVCPU; j++){
            int realCPU = currentStats[i].pcpuArr[j];
            // Finding the least weighted vCPU in maxUtilpCPU
            if(realCPU == maxUtilCpu){
                double tempTime = ((currentStats[i].vcpuTimeArr[j] - previousStats[i].vcpuTimeArr[j]) * 100.0)/ (timeDuration);
                if(tempTime < leastweightVCPU){
                    leastweightVCPU = tempTime;
                    // Record Domain of vCPU & vCPU Number which has the current lowest utilization
                    leastWeightCPUdomain = currentStats[i].domain;
                    leastWeightCPUnumber = j;
                }
            }
        }
    }
    printf("\t->Pinning vCPU %d in Domain %s from %d pCPU to pCPU %d \n", leastWeightCPUnumber,
           virDomainGetName(leastWeightCPUdomain), maxUtilCpu ,minUtilCpu);
    int pinResult = virDomainPinVcpu(leastWeightCPUdomain, leastWeightCPUnumber, &minUtilCpuMap, cpuMapSize);
    if( pinResult == 0){
        printf("\t->Pin Result: %s\n", pinResult == 0 ? "Success" : "Failure");
    }

    return;

}

void calculateCpuMetrics(int numberOfDomains, DomainStats *currentStats, DomainStats *previousStats, double *UsagePCPU, double timeDuration){
    //iterate through all domains and all vCPUs in each domain
    for(int i=0; i<numberOfDomains; i++){
        for(int j=0; j<currentStats[i].VCPUnumber; j++){
			//compare current and previous vCPU times to calcualte usage in percentage.
            double tempTime = ((currentStats[i].vcpuTimeArr[j] - previousStats[i].vcpuTimeArr[j]) * 100.0)/ (timeDuration);
            printf("\t-> Domain: %s, vCPU: %d, pCPU: %d, Usage: %f \n", virDomainGetName(currentStats[i].domain), j, 
                                                                    currentStats[i].pcpuArr[j], tempTime);
            UsagePCPU[currentStats[i].pcpuArr[j]] += tempTime;
        }
    }
}

//finds the most and least utilized pCPUs based on their usage percentages
int find(double *UsagePCPU, int PCPUnumber, int *maxUtilCpu, int *minUtilCpu){
    double max = 0.0;
    double min = DBL_MAX;

//iterate through all pCPUs to find the most and least utilized ones.
    for(int i=0; i < PCPUnumber; i++){
        double temp = UsagePCPU[i];
        if(temp > max){
            max = temp;
            *maxUtilCpu = i; //stores index of max CPU
        }
        if(temp < min){
            min = temp;
            *minUtilCpu = i; //stores index of min CPU
        }
    }
	//CHECKS if difference is less than THRESHOLD (20.0). If yes, then load-balancing is not required.
    if(max - min <= THRESHOLD){
        return 0;
    }
    return 1;
}


/*
DO NOT CHANGE THE FOLLOWING FUNCTION
*/
void signal_callback_handler()
{
	printf("Caught Signal");
	is_exit = 1;
}


void printUsage(double *UsagePCPU, int PCPUnumber){

    for(int i=0;i<PCPUnumber;i++){
        printf("\tpCPU %d's Usage: %f\n", i, UsagePCPU[i]);
    }
    return;
}

/*
DO NOT CHANGE THE FOLLOWING FUNCTION
*/
// orchestrates the vCPU scheduler's operation, which continuously monitors the system, collects statistics about active domains and physical CPUs, 
//calculates CPU metrics, and performs load balancing to ensure even distribution of workload across physical CPUs. 
//The loop continues until the program is manually terminated or number of VMs is ZERO.
int main(int argc, char *argv[])
{
	virConnectPtr conn;
	conn = virConnectOpen("qemu:///system");
	if(conn == NULL)
	{
		fprintf(stderr, "Failed to open connection\n");
		return 1;
	}

	if(argc != 2)
	{
		printf("Incorrect number of arguments\n Suggested Solution = Enter time in Seconds \n For example (2secs): Run ./vcpu_scheduler 2");
		return 0;
	}

	// Gets the interval passes as a command line argument and sets it as the STATS_PERIOD for collection of balloon memory statistics of the domains
	int interval = atoi(argv[1]);
	


	// Getting Host PCPU info - getting number of PCPUs
	virNodeInfo info;
	virNodeGetInfo(conn, &info);
	int PCPUnumber = info.cpus;


	//UsagePCPU is an array to store CPU usage percentages for each pCPU
	double *UsagePCPU = (double *)calloc(PCPUnumber, sizeof(double));
	if(UsagePCPU == NULL){
		printf("Memory Allocation Error (Check UsagePCPU) \n");
		return 1;
	}

	//Get vCPU statistics
	// 	- currentStats & previousStats = arrays to store active VM stats
	// - timeDuration = time interval for each iteration in nanoseconds*10^9
	// - previousDomains = variable to track number of active VMs in prev iter
	// - iter = iteration counter
	DomainStats *currentStats, *previousStats;
	// converting nanoseconds to seconds without using math.h 
	double timeDuration = atof(argv[1]) * 1000000000;
	int previousDomains = 0;
	int iter = 1;

	// Continuous Loop to periodically (specified) monitor the system.
	// - domainList = array to store the retrieved list of active VMs
	// - numberOfDomains = gets the number of active VMs during that iteration. if nothing, break loop.
	while(1){
		virDomainPtr *domainList;
		//Get all VMs which are BOTH active and running. These are the set flags. The virConenctListAllDomains handles memory allocation for domainList my itself.
		int numberOfDomains = virConnectListAllDomains(conn, &domainArr, VIR_CONNECT_LIST_DOMAINS_ACTIVE |
                         VIR_CONNECT_LIST_DOMAINS_RUNNING);
		if(numberOfDomains <= 0){
			break;
		}

		printf("number of PCPUs: %d \n", PCPUnumber);
		printf("Number of Domains: %d \n", numberOfDomains);

		//Stores statistics for current iteration
		currentStats = (DomainStats *)calloc(numberOfDomains, sizeof(DomainStats));
		initializeDomainStats(PCPUnumber, domainList, currentStats, numberOfDomains);

		//check to detect change in number of active VMs compared to previous iteration
		//if yes, allocate memory for previousStats and store the current number. 
		//if not, proceed. Calculate CPU metrics and perform load balancing (pinVCPU)
		//Basically we want to wait for algorithm to stabilize. if 2 iterations have the same number of VMs, then it is an indication of stability. Then proceed.
		if(numberOfDomains != previousDomains){
            previousStats = (DomainStats *)calloc(numberOfDomains, sizeof(DomainStats));
        }
        else{
            calculateCpuMetrics(numberOfDomains, currentStats, previousStats, UsagePCPU, timeDuration);
            pinVcpu(UsagePCPU, PCPUnumber, currentStats, previousStats, numberOfDomains, timeDuration);
        }

		//Copy current values to Previous values for next iteration
		previousDomains = numberOfDomains;
		memcpy(previousStats, currentStats, numberOfDomains*sizeof(DomainStats));
		printf("Iteration %d Completed!", iter);
		iter++;
		//reset each PCPUs array to ZERO for next iteration
		for(int i=0; i<PCPUnumber; i++){
			UsagePCPU[i] = 0.0;
		}
		//Sleep/wait until the specified time interval is complete before restarting
		sleep(interval);

		
	}



	// Closing the connection
	virConnectClose(conn);
	printf("Connection Closed!");
	return 0;
}




