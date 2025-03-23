#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <grpc++/grpc++.h>
#include "threadpool.h" // Include your thread pool header
#include "vendor.grpc.pb.h" // Include your generated gRPC service header

using namespace std;

class VendorClient {
public:
    VendorClient(shared_ptr<Channel> channel)
        : stub_(Vendor::NewStub(channel)) {}

    void QueryVendor(const string& product, int vendorId, double& bid) {
        ClientContext context;
        VendorRequest request;
        VendorReply reply;

        request.set_product(product);
        request.set_vendor_id(vendorId);

        CompletionQueue cq;
        Status status;

        unique_ptr<ClientAsyncResponseReader<VendorReply>> rpc(
            stub_->PrepareAsyncQueryVendor(&context, request, &cq)
        );

        rpc->StartCall();

        rpc->Finish(&reply, &status, (void*)1);

        void* got_tag;
        bool ok = false;

        GPR_ASSERT(cq.Next(&got_tag, &ok));

        if (status.ok() && ok) {
            bid = reply.bid();
        } else {
            cerr << "RPC failed: " << status.error_message() << endl;
            bid = -1.0; // Set a placeholder value for failed requests
        }
    }

private:
    unique_ptr<Vendor::Stub> stub_;
};

class StoreServiceImpl final : public Store::Service {
public:
    explicit StoreServiceImpl(int maxThreads, const vector<VendorClient>& vendorClients)
        : threadPool(maxThreads), vendors(vendorClients), numVendors(vendorClients.size()) {}

    grpc::Status QueryProductPrice(grpc::ServerContext* context,
                                   const StoreRequest* request,
                                   StoreReply* reply) override {
        string product = request->product();
        vector<pair<int, double>> bids;

        for (int vendorId = 1; vendorId <= numVendors; ++vendorId) {
            threadPool.enqueue([this, product, vendorId, &bids] {
                double bid;
                vendors[vendorId - 1].QueryVendor(product, vendorId, bid);
                
                // Lock the mutex to safely update the bids vector
                std::lock_guard<std::mutex> lock(bidsMutex);
                bids.emplace_back(vendorId, bid);
            });
        }

        // Wait for all tasks to complete
        threadPool.waitAll();

        // Process and collate the results in 'bids'

        for (const auto& bid : bids) {
            auto vendorBid = reply->add_bids();
            vendorBid->set_vendor_id(bid.first);
            vendorBid->set_bid(bid.second);
        }

        return grpc::Status::OK;
    }

private:
    ThreadPool threadPool;
    int numVendors;
    const vector<VendorClient>& vendors;
    std::mutex bidsMutex;
};

int main(int argc, char** argv) {
    if (argc != 3) {
        cerr << "Usage: " << argv[0] << " <ip address:port to listen on for clients> <maximum number of threads in threadpool>" << endl;
        return 1;
    }

    // Read vendor addresses from the file and create VendorClient instances
    const string vendorAddressesFile = "vendor_addresses.txt"; // Use the fixed filename
    const string serverAddress = argv[1];
    const int maxThreads = atoi(argv[2]);

    vector<string> vendorAddresses;
    ifstream vendorFile(vendorAddressesFile);
    string address;
    while (getline(vendorFile, address)) {
        vendorAddresses.push_back(address);
    }

    vector<VendorClient> vendors;
    for (const auto& address : vendorAddresses) {
        vendors.emplace_back(grpc::CreateChannel(address, grpc::InsecureChannelCredentials()));
    }

    // Create gRPC server and register the Store service
    grpc::ServerBuilder builder;
    builder.AddListeningPort(serverAddress, grpc::InsecureServerCredentials());
    StoreServiceImpl storeService(maxThreads, vendors);
    builder.RegisterService(&storeService);

    unique_ptr<grpc::Server> server(builder.BuildAndStart());
    cout << "Server listening on " << serverAddress << endl;
    server->Wait();

    return 0;
}
