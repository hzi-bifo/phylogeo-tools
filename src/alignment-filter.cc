#define BOOST_BIND_GLOBAL_PLACEHOLDERS
#include <iostream>
#include <fstream>
#include <string>
#include <boost/iostreams/filtering_streambuf.hpp>
#include <boost/iostreams/copy.hpp>
#include <boost/iostreams/filter/gzip.hpp>
#include <boost/iostreams/filter/lzma.hpp>
#include "tree.h"
#include <set>
namespace io = boost::iostreams;

int main(int argc, char* argv[]) {
	//cerr << "main" << endl;
	ifstream file(argv[1], ios_base::in | ios_base::binary);
	cerr << "file opened " << argv[1] << endl;
	io::filtering_streambuf<io::input> in;
	//in.push(gzip_decompressor());
	in.push(io::lzma_decompressor());
	in.push(file);
	std::istream incoming(&in);

	istream& file_alignment = endswith(argv[1],".xz") ? incoming : file; 

	vector<string> sample_ids;
	ifstream fi(argv[2]);
	for (string line; getline(fi, line); ) {
		if (line != "") {
			sample_ids.push_back(line);
			//cerr << "id=(" << line << ")" << endl;
		}
	}
	cerr << "sample_ids loaded " << sample_ids.size() << endl;

	set<string> samples(sample_ids.begin(), sample_ids.end()), samples_found;

	int dup_count=0;
	string seq, id;
	for (string line; getline(file_alignment, line); ) {
		//cerr << "L " << line << endl;
		if (line.size() == 0) continue;
		if (line[0] == '>') {
			if (id != "" && samples.find(id) != samples.end()) {
				if (samples_found.find(id) != samples_found.end()) {
					//cerr << "dup " << id << endl;
					dup_count++;
				} else {
				//cerr << "S " << id << endl;
					cout << ">" << id << endl;
					cout << seq << endl;
					samples_found.insert(id);
				}
			}
			//id = split(line.substr(1), '|')[0];
			id = line.substr(1);
			if (id.find("|") != string::npos) 
				id = id.substr(0, id.find("|"));
			seq = "";
			//cerr << "new id (" << id << ") " << id.size() << endl;
		} else {
			seq += line;
		}
	}
	if (id != "" && samples.find(id) != samples.end()) {
		if (samples_found.find(id) != samples_found.end()) {
			dup_count++;
		} else {
			cout << ">" << id << endl;
			cout << seq << endl;
			samples_found.insert(id);
		}
	}
	cerr << "saved " << samples_found.size() << " samples " << "dup: " << dup_count << endl;
	int samples_notfount_count = 0;
	for (auto const& s:samples) {
		if (samples_found.find(s) == samples_found.end()) {
			if (samples_notfount_count < 10)
				cerr << "W " << s << " ";
			samples_notfount_count++;
		}
	}
	if (samples_notfount_count > 0) {
		cerr << "not found " << samples_notfount_count << " samples" << endl;
	}

	return 0;
}
