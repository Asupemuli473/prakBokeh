/*
 *
 * Copyright 2015 gRPC authors.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 */
#define NCELLS_DOM01 327680
#define NCELLS_DOM02 135788
#define NLATS 360
#define PI 3.14159
#define HEIGHT_DIM_INDEX 1

#include <iostream>
#include <fstream>
#include <memory>
#include <string>
#include <numeric>

#include <grpcpp/grpcpp.h>
#include <netcdf>


#ifdef BAZEL_BUILD
#include "examples/protos/nc.grpc.pb.h"
#else
#include "nc.grpc.pb.h"
#endif

using grpc::Server;
using grpc::ServerBuilder;
using grpc::ServerContext;
using grpc::Status;
using nc::AggValuesPerLonRequest;
using nc::AggValuesPerLonReply;
using nc::MeshRequest;
using nc::MeshReply;
using nc::TrisRequest;
using nc::TrisReply;
using nc::TrisAggRequest;
using nc::NCService;

// Logic and data behind the server's behavior.
class ServiceImpl final : public NCService::Service {
private:
    std::vector<int> cellToLatLookupDOM01;
    std::vector<int> latCountDOM01;

    std::vector<int> cellToLatLookupDOM02;
    std::vector<int> latCountDOM02;

public:
    ServiceImpl(){
	cellToLatLookupDOM01 = std::vector<int>(NCELLS_DOM01);
	cellToLatLookupDOM02 = std::vector<int>(NCELLS_DOM02);

	latCountDOM01 = std::vector<int>(NLATS);
	latCountDOM02 = std::vector<int>(NLATS);

	for(int i = 0; i<NLATS; ++i){
	    std::ifstream file1, file2;
	    std::string line;
	    std::string filename1 = "../../dom01/dom01_lon_" + std::to_string(i) + "deg.dat", filename2 = "../../dom02/dom02_lon_" + std::to_string(i) + "deg.dat";
	    file1.open(filename1, std::ios::binary);
	    file2.open(filename2, std::ios::binary);
	    
	    if(file1.is_open() && file2.is_open()){
		while(getline(file1, line)){
		    int ncell = std::stoi(line);
		    cellToLatLookupDOM01[ncell] = i;
		    latCountDOM01[i]++;
		}
		file1.close();

		while(getline(file2, line)){
		    int ncell = std::stoi(line);
		    cellToLatLookupDOM02[ncell] = i;
		    latCountDOM02[i]++;
		}
		file2.close();		
	    }
	    else{
		std::cerr << "Unable to read file " << filename1 << " or " << filename2 << std::endl;
	    }
	}
	std::cout << "GRPC Server initialization complete" << std::endl;
    }

    Status GetHeightProfile (ServerContext* context, const nc::HeightProfileRequest* request,
			     nc::HeightProfileReply* reply) override {
	netCDF::NcFile ncFile;
	try {
	    ncFile.open(request->filename(), netCDF::NcFile::read);
	} catch(netCDF::exceptions::NcException &e) {
	    return Status(grpc::StatusCode::NOT_FOUND, "Unable to read nc file");
	}

	std::vector<int>* cellToLatLookup = request->dom() == nc::HeightProfileRequest_DOM_DOM01 ? &cellToLatLookupDOM01 : &cellToLatLookupDOM02;
	std::vector<int>* latCountLookup = request->dom() == nc::HeightProfileRequest_DOM_DOM01 ? &latCountDOM01 : &latCountDOM02;
	int ncells = request->dom() == nc::HeightProfileRequest_DOM_DOM01 ? NCELLS_DOM01 : NCELLS_DOM02;
    
	netCDF::NcVar var = ncFile.getVar(request->variable(), netCDF::NcGroup::Current);
	auto nheight = var.getDim(HEIGHT_DIM_INDEX).getSize();

	for(int i = 0; i<nheight; ++i){
	    auto hd = reply->add_result();
	    
	    // 3 dimensions
	    std::vector<size_t> start(3);
	    start[HEIGHT_DIM_INDEX] = i; // height
	    std::vector<size_t> count(3);
	    count[0] = 1;
	    count[1] = 1;
	    count[2] = ncells ;
    
	    std::vector<float> values(ncells);
	    var.getVar(start,count,&values[0]);
	
	    std::vector<float> data(NLATS);

	    for(int i = 0; i < ncells; ++i){
		data[(*cellToLatLookup)[i]] += values[i];
	    }

	    for(int i = 0; i < NLATS; ++i){
		auto val = nc::HeightProfileRequest_Op_MEAN == request->aggregatefunction() ? data[i] / (*latCountLookup)[i] : data[i];
		hd->add_data(val);
	    }
	}
    
	ncFile.close();
	return Status::OK;
    }
    
    
    Status GetAggValuesPerLon(ServerContext* context, const AggValuesPerLonRequest* request,
		    AggValuesPerLonReply* reply) override {
	netCDF::NcFile ncFile;
	try {
	    ncFile.open(request->filename(), netCDF::NcFile::read);
	} catch(netCDF::exceptions::NcException &e) {
	    return Status(grpc::StatusCode::NOT_FOUND, "Unable to read nc file");
	}

	int ncells = request->dom() == nc::AggValuesPerLonRequest_DOM_DOM01 ? NCELLS_DOM01 : NCELLS_DOM02;
	
	netCDF::NcVar var = ncFile.getVar(request->variable(), netCDF::NcGroup::Current);

	// 3 dimensions
	std::vector<size_t> start(3);
	start[1] = request->alt();
	std::vector<size_t> count(3);
	count[0] = 1;
	count[1] = 1;
	count[2] = ncells;
    
	std::vector<float> values(ncells);
	var.getVar(start,count,&values[0]);


	std::vector<int>* cellToLatLookup = request->dom() == nc::AggValuesPerLonRequest_DOM_DOM01 ? &cellToLatLookupDOM01 : &cellToLatLookupDOM02;
	std::vector<int>* latCountLookup = request->dom() == nc::AggValuesPerLonRequest_DOM_DOM01 ? &latCountDOM01 : &latCountDOM02;

	std::vector<float> data(NLATS);
	for(int i = 0; i < ncells; ++i){
	    data[(*cellToLatLookup)[i]] += values[i];
	}

	
	for(int i = 0; i < NLATS; ++i){
	    auto val = nc::AggValuesPerLonRequest_Op_MEAN == request->aggregatefunction() ? data[i] / (*latCountLookup)[i] : data[i];
	    reply->add_data(val);
	}

	ncFile.close();

	return Status::OK;
    }

    Status GetMesh(ServerContext* context, const MeshRequest* request,
		   MeshReply* reply) override {
	netCDF::NcFile ncFile;
	try {
	    ncFile.open(request->filename(), netCDF::NcFile::read);
	} catch(netCDF::exceptions::NcException &e) {
	    std::cerr << "couldnt read file" << std::endl;
	    return Status(grpc::StatusCode::NOT_FOUND, "Unable to read nc file");
	}

	int ncells = request->dom() == nc::MeshRequest_DOM_DOM01 ? NCELLS_DOM01 : NCELLS_DOM02;
	int nmesh = 3 * ncells;

	std::cout << "test";

	
	netCDF::NcVar clons = ncFile.getVar("clon_bnds", netCDF::NcGroup::Current);
	netCDF::NcVar clats = ncFile.getVar("clat_bnds", netCDF::NcGroup::Current);

	std::vector<double> valuesLons(nmesh), valuesLats(nmesh);
	std::vector<size_t> start(2), count(2);
	count[0] = ncells;
	count[1] = 1;

	clons.getVar(start,count,&valuesLons[0]);
	clats.getVar(start,count,&valuesLats[0]);

	start[1]++;
	clons.getVar(start,count,&valuesLons[ncells]);
	clats.getVar(start,count,&valuesLats[ncells]);
	
	start[1]++;
	clons.getVar(start,count,&valuesLons[2 * ncells]);
	clats.getVar(start,count,&valuesLats[2 * ncells]);

	double f = 180 / PI;
	
	for(int i = 0; i < nmesh; ++i){
	    reply->add_lons(f * valuesLons[i]);
	    reply->add_lats(f * valuesLats[i]);
	}
	
	ncFile.close();

	return Status::OK;
    }


    Status GetTris (ServerContext* context, const TrisRequest* request,
		    TrisReply* reply) override {
	netCDF::NcFile ncFile;
	try {
	    ncFile.open(request->filename(), netCDF::NcFile::read);
	} catch(netCDF::exceptions::NcException &e) {
	    std::cerr << "couldnt read file" << std::endl;
	    return Status(grpc::StatusCode::NOT_FOUND, "Unable to read nc file");
	}

	int ncells = request->dom() == nc::TrisRequest_DOM_DOM01 ? NCELLS_DOM01 : NCELLS_DOM02;

	netCDF::NcVar var = ncFile.getVar(request->variable(), netCDF::NcGroup::Current);

	std::vector<float> values(ncells);
	std::vector<size_t> start(3);
	start[1] = request->alt();

	std::vector<size_t> count(3);
	count[0] = 1;
	count[1] = 1;
	count[2] = ncells;

	var.getVar(start,count,&values[0]);

	for(auto v : values){
	    reply->add_data(v);
	}

	return Status::OK;
    }

    Status GetTrisAgg(ServerContext* context, const TrisAggRequest* request,
		   TrisReply* reply) override {
	netCDF::NcFile ncFile;
	try {
	    ncFile.open(request->filename(), netCDF::NcFile::read);
	} catch(netCDF::exceptions::NcException &e) {
	    std::cerr << "couldnt read file" << std::endl;
	    return Status(grpc::StatusCode::NOT_FOUND, "Unable to read nc file");
	}

	netCDF::NcVar var = ncFile.getVar(request->variable(), netCDF::NcGroup::Current);
	auto nheight = var.getDim(HEIGHT_DIM_INDEX).getSize();
	int ncells = request->dom() == nc::TrisAggRequest_DOM_DOM01 ? NCELLS_DOM01 : NCELLS_DOM02;
	
	std::vector<float> values(ncells);
	std::vector<size_t> start(3);
	std::vector<size_t> count(3);

	count[0] = 1;
	count[HEIGHT_DIM_INDEX] = 1;
	count[2] = ncells;

	std::vector<float> acc(ncells);
	for(int i = 0; i < nheight; ++i){
	    start[1] = i;
	    var.getVar(start,count,&values[0]);
	    std::transform (acc.begin(), acc.end(), values.begin(), acc.begin(), std::plus<float>());
	}

	for(int i = 0; i < ncells; ++i){
	    if(request->aggregatefunction() == nc::TrisAggRequest_Op_MEAN){
		reply->add_data(acc[i] / nheight);
	    }
	    else{
		reply->add_data(acc[i]);		
	    }

	}

	return Status::OK;
    }

    Status GetTrisAggStream(ServerContext* context, const TrisAggRequest* request,
			    grpc::ServerWriter<TrisReply>* writer) override {
	netCDF::NcFile ncFile;
	try {
	    ncFile.open(request->filename(), netCDF::NcFile::read);
	} catch(netCDF::exceptions::NcException &e) {
	    std::cerr << "couldnt read file" << std::endl;
	    return Status(grpc::StatusCode::NOT_FOUND, "Unable to read nc file");
	}

	netCDF::NcVar var = ncFile.getVar(request->variable(), netCDF::NcGroup::Current);
	auto nheight = var.getDim(HEIGHT_DIM_INDEX).getSize();
	int ncells = request->dom() == nc::TrisAggRequest_DOM_DOM01 ? NCELLS_DOM01 : NCELLS_DOM02;
	
	std::vector<float> values(nheight);
	std::vector<size_t> start(3);
	std::vector<size_t> count(3);
	count[0] = 1;
	count[HEIGHT_DIM_INDEX] = nheight;
	count[2] = 1;

	int bulk_size = 5000;
	for(int i = 0; i < ncells; ++i){
	    start[2] = i;
	    var.getVar(start,count,&values[0]);
	    auto acc = std::accumulate(values.begin(), values.end(), 0.0);
	    if(request->aggregatefunction() == nc::TrisAggRequest_Op_MEAN){
		acc /= nheight;
	    }
	    TrisReply reply = TrisReply();
	    reply.add_data(acc);

	    if(i % bulk_size == 0){
		writer->Write(reply);
	    }
	}

	return Status::OK;
    }

};

void RunServer() {
    std::string server_address("0.0.0.0:50051");
    ServiceImpl service;

    ServerBuilder builder;
    // Listen on the given address without any authentication mechanism.
    builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
    // Register "service" as the instance through which we'll communicate with
    // clients. In this case it corresponds to an *synchronous* service.
    builder.RegisterService(&service);
    // Finally assemble the server.
    std::unique_ptr<Server> server(builder.BuildAndStart());
    std::cout << "Server listening on " << server_address << std::endl;

    // Wait for the server to shutdown. Note that some other thread must be
    // responsible for shutting down the server for this call to ever return.
    server->Wait();
}

int main(int argc, char** argv) {
    RunServer();
}
