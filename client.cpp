/*
	Original author of the starter code
    Tanzir Ahmed
    Department of Computer Science & Engineering
    Texas A&M University
    Date: 2/8/20
	
	Please include your Name, UIN, and the date below
	Name: Matthews Naranjo
	UIN: 434004577
	Date: 9/25/2025
*/
#include "common.h"
#include "FIFORequestChannel.h"
#include <sys/wait.h>
#include <iomanip>
#include <unistd.h>
#include <cstring>
#include <fstream>
#include <vector>
#include <string>
#include <iostream>
#include <cstdlib>
#include <sstream>

using namespace std;


int main (int argc, char *argv[]) {
	int opt;
	int p = -1;
	double t = -1;
	int e = -1;
	int m = MAX_MESSAGE;
	bool new_chan = false;
	vector<FIFORequestChannel*> channels;
	
	string filename = "";
	while ((opt = getopt(argc, argv, "p:t:e:f:m:c")) != -1) {
		switch (opt) {
			case 'p':
				p = atoi (optarg);
				break;
			case 't':
				t = atof (optarg);
				break;
			case 'e':
				e = atoi (optarg);
				break;
			case 'f':
				filename = optarg;
				break;
			case 'm':
				m = atoi (optarg); //convert atoi to an integer
				break;
			case 'c':
				new_chan = true;
				break;
		}
	}


	//give arguments for the server
	// server needs './server', '-m' (buffer capacity, max # of byte can be sent), '<val for -m arg>', 'NULL'
	// fork
	// In the child, run execvp using the server arguments. (lab 1a and b)

	pid_t pid = fork();
	
    if (pid == 0) {
        //child process run server
        char arg_1[] = "./server";
		char arg_2[] = "-m";
		string m_str = to_string(m);
		char* server_args[] = {arg_1, arg_2, (char*)m_str.c_str(), NULL};
		execvp(server_args[0], server_args);
        perror("execvp failed");
        exit(1);
    } else if (pid < 0) {
        perror("fork failed");
        exit(1);
    }
    usleep(100000); // gives 100ms for the server to start


    FIFORequestChannel const_chan("control", FIFORequestChannel::CLIENT_SIDE);
	channels.push_back(&const_chan);

	if(new_chan){
		//send newChannel request to the server
		MESSAGE_TYPE nc = NEWCHANNEL_MSG;
		const_chan.cwrite(&nc, sizeof(MESSAGE_TYPE));
		//create a variable to hold the name
		// create the response from the server
		char new_chan_name[100];
        const_chan.cread(new_chan_name, sizeof(new_chan_name));

		//call the FIFORequestChannel constructor with the name from the server
		//(dynamically create channel so it exists outside the if statement)
		// push the new channel into the channels vector
		FIFORequestChannel* new_channel = new FIFORequestChannel(new_chan_name, FIFORequestChannel::CLIENT_SIDE);
        channels.push_back(new_channel);
	}

	FIFORequestChannel* chan = channels.back(); //get the last channel in the vector
	system("mkdir -p received");
	// single datapoint, only run p, t, e != -1
	// example data point request
	if (p != -1 && t != -1 && e != -1) {
		char buf[MAX_MESSAGE]; // 256
		datamsg x(p, t, e); //change from hardcoding to user's values
		
		memcpy(buf, &x, sizeof(datamsg));
		chan->cwrite(buf, sizeof(datamsg)); // question
		double reply;
		chan->cread(&reply, sizeof(double)); //answer
		cout << "For person " << p << ", at time " << fixed << setprecision(3) << t << ", the value of ecg " << e << " is " << fixed << setprecision(2) << reply << endl;
	}
	// Else, if p != -1, request 1000 datapoints.
	//loop over 1st 1000 lines
	// send request for ecg 1
	// send request for esg 2
	// write line to received/x1.csv (open the file however you want (fopen, filestream, etc))
	else if (p != -1 && t == -1 && e == -1) {
        ofstream outfile("received/x1.csv");
        
        for (int i = 0; i < 1000; i++) {
            double time_val = i * 0.004; //4ms intervals
            
            //Request ECG 1
            char buf[MAX_MESSAGE];
            datamsg msg1(p, time_val, 1);
            memcpy(buf, &msg1, sizeof(datamsg));
            chan->cwrite(buf, sizeof(datamsg));
            double ecg1_val;
            chan->cread(&ecg1_val, sizeof(double));
            
            //Request ECG 2
            datamsg msg2(p, time_val, 2);
            memcpy(buf, &msg2, sizeof(datamsg));
            chan->cwrite(buf, sizeof(datamsg));
            double ecg2_val;
            chan->cread(&ecg2_val, sizeof(double));
            
			if (i == 0) {
				outfile << "0," << ecg1_val << "," << ecg2_val << endl;
			} else {
				stringstream ss;
				ss << fixed << setprecision(3) << time_val;
				outfile << ss.str() << "," << ecg1_val << "," << ecg2_val << endl;
			}
        }
        outfile.close();
        cout << "Wrote first 1000 data points for person " << p << " to received/x1.csv" << endl;
    }
	else if (!filename.empty()) {
		// sending a non-sense message, you need to change this
		filemsg fm(0, 0);
		
		int len = sizeof(filemsg) + (filename.size() + 1);
		char* buf2 = new char[len]; //create buf2 size of filemsg + filename + null terminator
		memcpy(buf2, &fm, sizeof(filemsg)); //copy file message into the buffer
		strcpy(buf2 + sizeof(filemsg), filename.c_str()); //then copy file name into the buffer
		chan->cwrite(buf2, len);  //then send buffer to the server. I want the file length;

		__int64_t filesize = 0;
		chan->cread(&filesize, sizeof(__int64_t));
		cout << "File size: " << filesize << " bytes" << endl;

		//create the output file
        string output_path = "received/" + filename;
        FILE* outfile = fopen(output_path.c_str(), "wb");
        if (!outfile) {
            cerr << "Cannot create output file: " << output_path << endl;
            delete[] buf2;
            exit(1);
        }

		char* buf3 = new char[m]; //create buffer of size biff capacity (m)

		__int64_t remaining = filesize;
        __int64_t offset = 0;
        while (remaining > 0) {
            int segment_size = min((__int64_t)m, remaining);

			// Loop over the segments in the file filesize / buff capacity (m).
			// create filemsg instance 
			filemsg* file_req = (filemsg*)buf2;
			file_req->offset = offset;//set offset in the file
			file_req->length = segment_size;//set length. be careful of the last segment
			//send the request (buf2)
			chan->cwrite(buf2, len);
			//receive the response (response buffer needs to be different from the request buffer)
			int bytes_read = chan->cread(buf3, m);
			//cread into buf3 length file_req->len
			fwrite(buf3, 1, bytes_read, outfile);
			//write buf3 into file: received/filename

			offset += bytes_read;
			remaining -= bytes_read;
		}

		fclose(outfile);
		delete[] buf2;
		delete[] buf3;

		cout << "File " << filename << " transferred successfully to " << output_path << endl;
	}

	//if necessary, close and delete the new channel
	if(new_chan && channels.size() > 1){
		//do your close and deletes (similar to whats below)
		MESSAGE_TYPE quit_msg = QUIT_MSG;
        channels.back()->cwrite(&quit_msg, sizeof(MESSAGE_TYPE));
        delete channels.back();
        channels.pop_back();
	}
	
	// closing the channel    
    MESSAGE_TYPE quit_msg2 = QUIT_MSG;
	const_chan.cwrite(&quit_msg2, sizeof(MESSAGE_TYPE));
    // Wait for server process to finish
    wait(NULL);
	return 0;
}
