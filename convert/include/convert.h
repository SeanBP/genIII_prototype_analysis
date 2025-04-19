#ifndef __CONVERT__
#define __CONVERT__

#include <iostream>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <string>
#include <vector>
#include <map>

#include "TFile.h"
#include "TTree.h"

#include "utilities.h"
#include "calo.h"

using namespace std;

vector<string> split(string in)
{
    vector<string> out;
    istringstream ss(in);
    string word;
    while (ss >> word)
	out.push_back(word);
    return out;
}

////////////////////////////////////////////////////////////////////////
class boardReadout {
  public:
    boardReadout(const int bid, const double ts) { id = bid; TS =ts; }
    ~boardReadout() {}
    int getId() { return id; }
    int getnChannels() { return nChannels; }
    double getTS() { return TS; }
    map<int, int> getLG() { return LG; }
    map<int, int> getHG() { return HG; }
    void addChannel(int ch, int vLG, int vHG)
    {
	LG[ch] = vLG;
	HG[ch] = vHG;
	nChannels++;
    }

  private:
    int id = -1;
    double TS = -1;
    int nChannels = 0;
    map<int, int> LG;
    map<int, int> HG;
};

////////////////////////////////////////////////////////////////////////
class listReader {
  public:
    listReader(string f) { listFile = f; }
    ~listReader() {}
    bool isEof() { return eof; }
    time_t getStartTime();
    void read(const int n);
    void addBoard(boardReadout *b);
    int  getBoards(const int n, vector<boardReadout*>&);

  private:
    ifstream fin;
    string listFile;
    string line;
    vector<string> fields;
    int li = 0;
    bool eof = false;
    boardReadout* board = 0;
    vector<boardReadout*> boards;
    int nGoods = 0;
    int nBads = 0;
};

////////////////////////////////////////////////////////////////////////
struct event {
    double TS;
    map<int, int> LG;
    map<int, int> HG;
};

////////////////////////////////////////////////////////////////////////
class eventBuilder {
  public:
    eventBuilder(listReader* r) { reader = r; }
    ~eventBuilder() {}
    void addBoard(boardReadout*);
    void getTimeDiff();
    void build();
    int  getnEvents() { return events.size(); }
    int  getEvents(const int n, vector<event*>& ret);

  private:
    listReader* reader;
    int nEvents = 0;
    int nGoods = 0;
    int nBads = 0;
    map<int, double> timeDiff;
    map<int, vector<boardReadout*>> boards;
    map<int, vector<double>> TS;
    vector<event*> events;
};

////////////////////////////////////////////////////////////////////////
class treeMaker {
  public:
    treeMaker(eventBuilder* b) { builder = b; }
    ~treeMaker() {}
    void setStartTime(time_t t) { st = t; }
    void setOfName(string n) { ofName = n; }
    void init();
    void fill();
    void write();

  private:
    eventBuilder *builder = NULL;

    int nEvents = 0;
    time_t st;
    double TS, preTS = 1e32;
    float rate;

    map<int, pair<int, int>> rawADC;

    string ofName;
    TFile *fout = NULL;
    TTree *traw = NULL;
};


//////////////////////////////////////////////////////////////////////
time_t listReader::getStartTime() {
    if (!fin.is_open())
	fin.open(listFile);

    // skip the first 9 lines
    while (li<6)
    {
	li++;
	getline(fin, line);
    }
    // get the start time
    li++;
    getline(fin, line);
    fields = split(line);
    string st = fields[8] + "-" + fields[5] + "-" + fields[6] + " " + fields[7];
    tm t{};
    istringstream ss(st);
    ss >> get_time(&t, "%Y-%b-%d %H:%M:%S");
    while (li<9)
    {
	li++;
	getline(fin, line);
    }

    time_t startTime = mktime(&t); // UTC time
    return startTime;
}

void listReader::read(const int nRequest)
{
    int bid;
    double ts;
    int ch, HG, LG;
    while(boards.size() < nRequest && getline(fin, line))
    {
	li++;
	fields.clear();
	fields = split(line);
	const int n = fields.size();
	if (7 == n)
	{
	    addBoard(board);

	    bid = stoi(fields[0]);
	    ch  = stoi(fields[1]);
	    LG  = stoi(fields[2]);
	    HG  = stoi(fields[3]);
	    ts  = stod(fields[4])*us;
	    board = new boardReadout(bid, ts);
	}
	else if (4 == n)
	{
	    bid = stoi(fields[0]);
	    ch  = stoi(fields[1]);
	    LG  = stoi(fields[2]);
	    HG  = stoi(fields[3]);
	}
	else
	{
	    cerr << ERROR << "Invalid value in line " << li << endl;
	    continue;
	}
	ch += calo::preChannels[bid];
	board->addChannel(ch, LG, HG);
    }
    
    if (boards.size() < nRequest)
    {
	// the last board
	addBoard(board);
	eof = true;
    }
    cout << INFO << nGoods << "/" << nBads << "  good/bad board entries read" << endl;
}

void listReader::addBoard(boardReadout *b)
{
    if (!b)
	return;

    int bid = b->getId();
    if (b->getnChannels() != calo::nCAENChannels[bid])
    {
	nBads++;
	cerr << WARNING << "bad board record in board " << bid << ", "
	     << b->getnChannels() << "/" << calo::nCAENChannels[bid] << " recorded; timestamps: " << b->getTS() << endl;
	return;
    }
    nGoods++;
    boards.push_back(b);
}

int listReader::getBoards(const int nRequest, vector<boardReadout*>& ret)
{
    ret.clear();
    int n =  nRequest;
    while (!eof && boards.size() < n)
	read(n - boards.size());

    if (boards.size() < n && eof)
	n = boards.size();

    vector<boardReadout*>::const_iterator first = boards.begin();
    ret = {first, first+n};
    boards.erase(first, first+n);
    return ret.size();
}



//////////////////////////////////////////////////////////////////////
void eventBuilder::addBoard(boardReadout* b)
{
    int bid = b->getId();
    boards[bid].push_back(b);
    TS[bid].push_back(b->getTS());
}

void eventBuilder::getTimeDiff()
{
    // map<int, pair<double, double>> tdRange = {
    //     {1, {1.9*ms, 2.1*ms}},
    //    	{2, {-0.8*ms, -0.6*ms}},
    //     {3, {46*ms, 62*ms}},
    //     {4, {78*ms, 82*ms}},
    //     {5, {98*ms, 102*ms}},
    // };
    pair<double, double> tdRange = {-5*ms, 5*ms};
    vector<int> ei; // event index
    for (int ci=0; ci<calo::nCAENs; ci++)
	ei.push_back(0);

    while (ei[0] < boards[0].size())	
    {
	double ts0 = TS[0][ei[0]];
	bool findEvent = true;
	for (int ci=1; ci<calo::nCAENs; ci++)
	{
	    bool findMatch = false;
	    while (ei[ci] < boards[ci].size())
	    {
		double ts = TS[ci][ei[ci]];
		if (ts > ts0*1000)	// some insane values
		{
		    ei[ci]++;
		    continue;
		}

		double diff = ts - ts0;
		if (tdRange.first <= diff and diff <= tdRange.second)
		{
		    findMatch = true;
		    break;
		}
		else
		{
		    if (ts > ts0)
			break;
		    else
			ei[ci]++;
		}
	    }

	    if (not findMatch)
	    {
		findEvent = false;
		break;
	    }
	}
	if (findEvent)
	{
	    for (int ci=0; ci<calo::nCAENs; ci++)
	    {
		timeDiff[ci] = ts0 - TS[ci][ei[ci]];
		cout << INFO << "first event at CAEN unit " << ci << ": " << TS[ci][ei[ci]]
		    << ", time difference to unit 0: " << timeDiff[ci]/us << endl;
	    }
	    break;
	}
	ei[0]++;
    }
}

void eventBuilder::build()
{
    if (not timeDiff.size())
    {
	getTimeDiff();
	if (timeDiff.size() != calo::nCAENs)
	{
	    cerr << FATAL << "can't find out the time difference" << endl;
	    cout << INFO << "here are the first 10 records" << endl;
	    for (int i=0; i<10; i++)
	    {
		for (int ci=0; ci<calo::nCAENs; ci++)
		{
		    if (i < boards[ci].size())
			cout << "\t" << ci << "\t" << boards[ci][i]->getTS();
		}
		cout << endl;
	    }
	    exit(4);
	}
    }

    for (int ci=1; ci<calo::nCAENs; ci++)
    {
	for (auto& ts : TS[ci])
	    ts += timeDiff[ci];
    }

    vector<int> ei;
    map<int, boardReadout*> eventCandidate;
    bool hasCandidate = true;
    for (int ci=0; ci<calo::nCAENs; ci++)
    {
	ei.push_back(0);
	hasCandidate &= (boards[ci].size() > 0);
	eventCandidate[ci] = NULL;
    }

    double ts;
    event* evt = NULL;
    while (hasCandidate)
    {
	double ts0 = TS[0][ei[0]];
	for (int ci=1; ci<calo::nCAENs; ci++)
	{
	    ts = TS[ci][ei[ci]];
	    if (ts < ts0)
		ts0 = ts;
	}

	bool findEvent = true;
	for (int ci=0; ci<calo::nCAENs; ci++)
	{
	    ts = TS[ci][ei[ci]];
	    while (ts > ts0*1000)
	    {
		delete boards[ci][ei[ci]];
		ei[ci]++;
		if (ei[ci] == boards[ci].size())
		{
		    ei[ci]--;
		    break;
		}
		else
		    ts = TS[ci][ei[ci]];
	    }
	    if ((ts - ts0) < 10*us)
	    {
		eventCandidate[ci] = boards[ci][ei[ci]];
	    }
	    else
	    {
		findEvent = false;
	    }
	}
	nEvents++;

	if (findEvent)
	{
	    nGoods++;
	    evt = new event();
	    evt->TS = ts0/s;
	    for (int ci=0; ci<calo::nCAENs; ci++)
	    {
		for (auto const ele : boards[ci][ei[ci]]->getLG())
		    (evt->LG).insert(ele);
		for (auto const ele : boards[ci][ei[ci]]->getHG())
		    (evt->HG).insert(ele);
	    }
	    events.push_back(evt);
	}
	else
	{
	    nBads++;
	    cerr << WARNING << "bad event " << nBads;
	    for (int ci=0; ci<calo::nCAENs; ci++)
		cerr << "\t" << ci << ": " << (to_string) (boards[ci][ei[ci]]->getTS());
	    cerr << endl;
	}

	for (int ci=0; ci<calo::nCAENs; ci++)
	{
	    if (eventCandidate[ci])
	    {
		delete eventCandidate[ci];
		eventCandidate[ci] = NULL;
		boards[ci][ei[ci]] = NULL;
		ei[ci]++;
		hasCandidate &= (ei[ci] < boards[ci].size());
	    }
	}
    }

    // remove the used records
    for (int ci=0; ci<calo::nCAENs; ci++)
    {
	boards[ci].erase(boards[ci].begin(), boards[ci].begin()+ei[ci]);
	TS[ci].erase(TS[ci].begin(), TS[ci].begin()+ei[ci]);
    }
    // recover the TS
    for (int ci=1; ci<calo::nCAENs; ci++)
    {
	for (auto& ts : TS[ci])
	    ts -= timeDiff[ci];
    }

}

int eventBuilder::getEvents(const int nRequest, vector<event*>& ret)
{
    ret.clear();
    if (reader->isEof() && 0 == events.size())
	return 0;

    int n = nRequest;
    while (events.size() < n && !reader->isEof())
    {
	vector<boardReadout*> vb;
	reader->getBoards(calo::nCAENs*(n-events.size()), vb);
	for (boardReadout* b : vb)
	    addBoard(b);
	build();
    }

    if (reader->isEof())
    {
	int left = 0;
	for (int ci=0; ci<calo::nCAENs; ci++)
	{
	    int n = boards[ci].size();
	    if (left < n)
		left = n;
	    for (int i=0; i<n; i++)
		delete boards[ci][i];
	    boards[ci].clear();
	}
	nEvents += left;
    }

    if (events.size() < n)
    {
	n = events.size();
	cout << INFO << nGoods << "/" << nEvents << " built." << endl;
    }

    vector<event*>::const_iterator first = events.begin();
    ret = {first, first+n};
    events.erase(first, first+n);
    return ret.size();
}




//////////////////////////////////////////////////////////////////////
void treeMaker::init()
{
    fout = new TFile(ofName.c_str(), "recreate");
    traw = new TTree("raw", "raw ADC values");	 // owned by fout
    traw->Branch("TS", &TS);
    for (int ch=0; ch<calo::nChannels; ch++)
    {
	rawADC[ch] = {0, 0};
	traw->Branch(Form("ch_%d", ch), &rawADC[ch], "LG/I:HG/I");
    }
    traw->Branch("rate",  &rate);
}

void treeMaker::fill()
{
    if (!builder)
    {
	cerr << ERROR << "No event builder specified." << endl;
	return;
    }

    vector<event*> ve;
    while(builder->getEvents(10000, ve) != 0)
    {
	for (auto &evt : ve)
	{
	    nEvents++;

	    TS = evt->TS + st;
	    rate = 1/(TS - preTS);
	    preTS = TS;
	    for (int ch=0; ch<calo::nChannels; ch++)
	    {
		rawADC[ch] = {evt->LG[ch], evt->HG[ch]};
	    }
	    traw->Fill();
	    delete evt;
	}
	cout << INFO << nEvents << " events filled" << endl;
    }
}

void treeMaker::write()
{
    traw->Write();
    fout->Close();

    delete fout;
}


#endif
