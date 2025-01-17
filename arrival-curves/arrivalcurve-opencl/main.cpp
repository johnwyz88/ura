#include "../simple-opencl/simpleCL.h"
#include <iostream>
#include <ctime>
#include <string>
#include <stdlib.h>
#include <vector>
#include <fstream>
#include <sstream>
#include <sys/stat.h>
using namespace std;

#define DEVICE_ID 0

//O(mn) solution (m = time interval size, n = event size)
typedef struct step {
	float events;
	float left;
	float right;
} step;

vector<float> e;
float *floatevents;
vector<step>max_vec, min_vec;
float max_event, maxc, minc, prev_minc, next_max, next_min, getcount = 0, totalcount = 0;

void get_min(float invl, bool enable){
	getcount++;
	float upper = 0;
	minc = e.size();
	//find number of events between 0 and invl
	while(e[upper] <= invl){
		upper++;			
	}
	float events = upper;
	if(minc > events) minc = events;
	float lower = 0, udiff, ldiff;
	bool upset = true, loset = true, delay = false;
	while(upper < e.size()){
		if(e[lower] > max_event - invl) break;		
		// how far to get to next upper event
		if(upset){
			upset = false;
			if(lower == 0)
				udiff = e[upper] - invl;
			else
				udiff = e[upper] - e[upper - 1];
		}
		if(loset){
			loset = false;
			if(lower == 0)
				ldiff = e[lower] - 0; // how far to get to first lower event (+ 1 for post clean up)
			else
				ldiff = e[lower] - e[lower - 1];  // how far to get to next lower event
		}
		//cout << "ldiff " << ldiff << " udiff " << udiff << endl;
		if(udiff < ldiff){
			upper++;
			ldiff -= udiff;
			upset = true;
			events++;
			if(delay){
				events--;
				delay = false;
			}
		} else if(udiff > ldiff){
			lower++;
			udiff -= ldiff;
			loset = true;
			if(delay) events--;
			else delay = true;
		} else{
			upper++;
			lower++;
			upset = true;
			loset = true;
			events++;
			if(delay) events--;
			delay = true;
		}
		if(minc > events - 1) minc = events - 1; //IMPORTANT: the reason is the delay subtract
		if(minc == prev_minc && enable) break;		
	}
	prev_minc = minc;
}

void get_counts(float invl){
	getcount++;
	minc = e.size();
	maxc = 0;
	float upper = 0;
/*
	//opencl kernel call
	int found;
	float value = (float)invl;
        // SimpleOpenCL types declaration 
        sclHard* hardware;
        sclSoft software;

        // NDRange 2D size initialization
        size_t global_size[2];
        size_t local_size[2];
        size_t dataLength=e.size();
        size_t dataSize=sizeof(float)*dataLength;
    
        global_size[0]=dataLength; global_size[1]=1;
        local_size[0]=1; local_size[1]=1;
        //local_size[0]=1 might be necessary for CPU devices on apple machines

        // Hardware and Software initialization ##### HERE STARTS THE SimpleOpenCL CODE ####
        found=0;
        hardware = sclGetAllHardware(&found);
        software = sclGetCLSoftware("arrival.cl","arrival",hardware[DEVICE_ID]);

        // Kernel execution
        sclManageArgsLaunchKernel( hardware[DEVICE_ID], software,
                                   global_size, local_size,
                                   "%R %a %N",
                                   dataSize, (void*)floatevents, sizeof(float), (void*)&value, sizeof(float));
	//kernel call ends 
*/
	//find number of events between 0 and invl
	while(e[upper] <= invl){
		upper++;			
	}
	float events = upper;
	if(maxc < events) maxc = events;
	if(minc > events) minc = events;
	float lower = 0, udiff, ldiff;
	bool upset = true, loset = true, delay = false;
	while(upper < e.size()){
		if(e[lower] > max_event - invl){
			//last round
			events += e.size() - upper - 1;
			if(maxc < events) maxc = events;
			break;
		}
		// how far to get to next upper event
		if(upset){
			upset = false;
			if(lower == 0)
				udiff = e[upper] - invl;
			else
				udiff = e[upper] - e[upper - 1];
		}
		if(loset){
			loset = false;
			if(lower == 0)
				ldiff = e[lower] - 0; // how far to get to first lower event (+ 1 for post clean up)
			else
				ldiff = e[lower] - e[lower - 1];  // how far to get to next lower event
		}
		//cout << "ldiff " << ldiff << " udiff " << udiff << endl;
		if(udiff < ldiff){
			upper++;
			ldiff -= udiff;
			upset = true;
			events++;
			if(delay){
				events--;
				delay = false;
			}
		} else if(udiff > ldiff){
			lower++;
			udiff -= ldiff;
			loset = true;
			if(delay) events--;
			else delay = true;
		} else{
			upper++;
			lower++;
			upset = true;
			loset = true;
			events++;
			if(delay) events--;
			delay = true;
		}
		if(maxc < events) maxc = events;
		if(minc > events - 1) minc = events - 1; //IMPORTANT: the reason is the delay subtraction
	}	
}

float bin_max_steps(float lower, float upper, float curr){
	long long mid = (lower + upper) / 2;
	get_counts((float)mid);
	if(maxc == curr){
		get_counts(mid + 1);
		if(maxc > curr){
			next_max = maxc;
			return mid;
		} else {
			return bin_max_steps(mid + 1, upper, curr);
		}
	} else {
		return bin_max_steps(lower, mid - 1, curr);
	}
}

float bin_min_steps(float lower, float upper, float curr, bool enable){
	long long mid = (lower + upper) / 2;
	get_min((float)mid, enable);
	if(minc == curr){
		get_min(mid + 1, enable);
		if(minc > curr){
			next_min = minc;
			return mid;
		} else {
			return bin_min_steps(mid + 1, upper, curr, true);
		}
	} else {
		return bin_min_steps(lower, mid - 1, curr, false);
	}
}


int main(int argc, char *argv[]){
	if ( argc != 5 || atoll(argv[1]) <= 0 || atoll(argv[2]) <= atoll(argv[1]) ){
		cout << "usage: [minimum interval ( >= 1 ns )] [maximum interval] [input] [output]\n";
		return 0;
	}
    clock_t start = clock();
	ifstream infile(argv[3]);
	string tmp, line;
	if(!infile.good()){
		infile.close();
		cout << "input file does not exist!\n";
		return 0;
	}
	while (std::getline(infile, line)) {
		istringstream iss(line);
		if (!(iss >> tmp)) break; // error
		e.push_back(stof(tmp));
	}
	infile.close();
	ofstream cout;
	cout.open(argv[4]);
	max_event = e[e.size() - 1];
	if(atoll(argv[2]) > max_event){
		cout << "[maximum interval] is larger than the max event time. Exiting...\n";
		return 0;
	}
	floatevents = new float[e.size()];
	long c = 0;
	for(auto it = e.begin(); it != e.end(); ++it){
		floatevents[c] = *it;
	}

	float curr = atoll(argv[1]), last = atoll(argv[2]);
	get_counts(last);
	float last_max = maxc, last_min = minc;
	get_counts(curr);
	float curr_max = maxc, curr_min = minc, next;
	step s;
	while(curr_max != last_max){
		next = bin_max_steps(curr, last, curr_max);		
		cout << "maxc = " << curr_max << " for intervals between " << curr << " and " << next << endl;
		cout << "called getcount() " << getcount << " times" << endl;
		totalcount += getcount;
		getcount = 0;
		s.events = curr_max;
		s.left = curr;
		s.right = next;
		max_vec.push_back(s);
		curr = next + 1;
		curr_max = next_max;
	}
	cout << "maxc = " << curr_max << " for intervals between " << curr << " and " << last << endl;
	cout << "called getcount() " << getcount << " times" << endl << endl;
	totalcount += getcount;
	getcount = 0;
	s.events = curr_max;
	s.left = curr;
	s.right = last;
	max_vec.push_back(s);
		
	curr = atoll(argv[1]);
	while(curr_min != last_min){
		next = bin_min_steps(curr, last, curr_min, false);
		cout << "minc = " << curr_min << " for intervals between " << curr << " and " << next << endl;
		cout << "called getcount() " << getcount << " times" << endl;
		totalcount += getcount;
		getcount = 0;
		s.events = curr_min;
		s.left = curr;
		s.right = next;
		min_vec.push_back(s);
		curr = next + 1;
		curr_min = next_min;
	}
	cout << "minc = " << curr_min << " for intervals between " << curr << " and " << last << endl;
	cout << "called getcount() " << getcount << " times" << endl << endl;
	totalcount += getcount;
	getcount = 0;
	s.events = curr_min;
	s.left = curr;
	s.right = last;
	min_vec.push_back(s);
	long double duration = (long double) (std::clock() - start) / CLOCKS_PER_SEC;
	cout << "Duration: " << duration << endl;
	cout << "total number of getcount(): " << totalcount << endl;
	cout << "time spent per getcount(): " << duration / totalcount << endl;
	cout.close();
        return 0;
    
}
