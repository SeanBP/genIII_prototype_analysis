#include <unistd.h>
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <map>
#include <vector>
#include <ctime>
#include "TFile.h"
#include "TTree.h"
#include "calo.h"
#include "utilities.h"
#include "convert.h"

using namespace std;

void usage()
{
    cout << INFO << "usage" << endl;
    cout << "\t./convert1 -c nCh1,nCh2,... -i listFile -o outFileName" << endl;
}

int main(int argc, char *argv[])
{
    string nCh_s;
    string listFile;
    string pedFile;
    string outName;
    char opt;
    while ((opt = getopt(argc, argv, "c:i:o:")) != -1)
	switch(opt)
	{
	    case 'c':
		nCh_s = optarg;
		break;
	    case 'i':
		listFile = optarg;
		break;
	    case 'o':
		outName = optarg;
		break;
	    default:
		usage();
		exit(0);
	}

    if (!nCh_s.empty())
    {
	string ch;
	vector<int> nCh;
	for(stringstream sst(nCh_s); getline(sst, ch, ','); )
	    nCh.push_back(stoi(ch));
	calo::setnCAENChannels(nCh);
    }

    if (!fileExists(listFile.c_str()))
    {
	cerr << FATAL << "list file doesn't exist: " << listFile << endl;
	exit(4);
    }
    listReader* reader = new listReader(listFile);

    eventBuilder *builder = new eventBuilder(reader);
    treeMaker *maker = new treeMaker(builder);
    maker->setStartTime(reader->getStartTime());
    maker->setOfName(outName);
    maker->init();
    maker->fill();
    maker->write();

    delete builder;
    delete maker;
    delete reader;
}
