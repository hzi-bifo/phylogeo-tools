#include <string>
#include <iostream>
#include <fstream>
#include <random>
#include <regex>
#include <ranges>
#include <chrono>
#include <iomanip> // for setbase(), works for base 8,10 and 16 only

using namespace std;

vector<string> split_string(const string& l, char splitter = '\t') {
	// cerr << "split_string: " << l << " ... " << endl;
	vector<string> x;
	for (size_t st = 0; st != string::npos; ) {
		size_t i = l.find(splitter, st);
		if (i == string::npos) {
			x.push_back(l.substr(st));
			st = i;
		} else {
			x.push_back(l.substr(st, i - st));
			st = i + 1;
		}
	}
	// cerr << "split_string: " << l << " " << x << endl;
	return x;
}


string replace_all(std::string text, const std::string& from, const std::string& to) {
    for (auto at = text.find(from, 0); at != std::string::npos;
        at = text.find(from, at + to.length())) {
        text.replace(at, from.length(), to);
    }
    return text;
}

template<typename T>
string string_join(const T& x, char splitter = '\t') {
	string r;
	bool is_first = true;
	for (const string& y : x) {
		if (!is_first)
			r += splitter;
		r += y;
		is_first = false;
	}
	return r;
}

struct Metadata2 {
	string id, name, date, location, location_add, pangolin, aasub, submission_date;
	Metadata2(string _id = "", string _name = "", string _date = "", string _location = "", string _location_add = "", string pangolin ="", string aasub="", string submission_date="") :
		id(_id), name(_name), date(_date), location(_location), location_add(_location_add), pangolin(pangolin), aasub(aasub), submission_date(submission_date) {}

	string country() const {
		vector<string> lparts = split_string(location, '/');
		if (lparts.size() >= 2)
			return trim(lparts[1]);
		return "";
	}
};

struct MetadataReader {
    string fn;
    ifstream file;

    MetadataReader(string fn) : fn(fn), file(fn) {
    }
    
    int name_index = -1, id_index = -1, date_index = -1, location_index = -1, collection_date_index = -1, location_addition_index = -1, pangolin_index=-1, aasub_index = -1, submission_date_index=-1;
    string line = "";
    Metadata2 metadata;
    vector<string> metadata_line_split, metadata_header_line_split;
    bool next() {
        for (; line != "" || getline(file, line); ) {
            //cerr << "line read " << line << endl;
            if (line.size() == 0) {
                line = "";
                continue;
            }
            char separator = 0;
            if (separator == 0) {
                if (line.find('\t') != string::npos) 
                    separator = '\t';
                else if (line.find(',') != string::npos) 
                    separator = ',';
            }
            //vector<string> x = split(trim(line), separator);
            vector<string> x = split_string(line, separator);
            //cerr << "line split " << x << endl;
            if (id_index == -1) {
                //cerr << "reading header " << x << endl;
                name_index = indexOf(x, {"Virus name", "Virus.name", "virus_name", "strain", "sequence_name"});
                id_index = indexOf(x, {"Accession ID", "Accession.ID", "accession_id", "accession", "sequence_name"});
                date_index = indexOf(x, {"Collection date", "Collection.date", "collection_date", "date", "sample_date"});
                location_index = indexOf(x, {"Location", "country"});
                location_addition_index = indexOf(x, {"Additional.location.information", "Additional location information", "state"});
                //collection_date_index = indexOf(x, {"Submission date"});
                collection_date_index = indexOf(x, {"Collection date", "Collection.date", "sample_date", "date"});
                pangolin_index = indexOf(x, {"Pango lineage", "Pango.lineage", "lineage"});
                submission_date_index = indexOf(x, {"Submission.date", "Submission date", "date"});
                aasub_index = indexOf(x, {"AA Substitutions"});
                line = "";
                //cerr << "header loaded " << x << endl;
                metadata_header_line_split = x;
                continue;
            } else {
                if (id_index >= (int)x.size() ) {
                    cerr << "x short " << x.size() << " id=" << id_index << endl;
                }
                if (name_index >= (int)x.size() ) {
                    cerr << "x short " << x.size() << " name=" << name_index << endl;
                }
                if (date_index >= (int)x.size() ) {
                    cerr << "x short " << x.size() << " date_index=" << date_index << endl;
                }
                if (collection_date_index >= (int)x.size() ) {
                    cerr << "x short " << x.size() << " collection_date_index=" << collection_date_index << endl;
                }
                if (location_index >= (int)x.size() ) {
                    cerr << "x short " << x.size() << " location_index=" << location_index << endl;
                }
                if (location_addition_index >= (int)x.size() ) {
                    cerr << "x short " << x.size() << " location_addition_index=" << location_addition_index << " " << x << endl;
                }
                if (pangolin_index >= (int)x.size() ) {
                    cerr << "x short " << x.size() << " pangolin_index=" << pangolin_index << endl;
                }
                // if (aasub_index >= (int)x.size() ) {
                //     cerr << "x short " << x.size() << " aasub_index=" << aasub_index << " " << x << endl;
                //     exit(0);
                // }

		/*
		for (const auto &y:x) {
			cerr << "|" << y;
		}
		cerr << "|" << endl;
		*/


                metadata_line_split = x;
                metadata = Metadata2(x[id_index], x[name_index], x[date_index].size() != 10 ? x[collection_date_index] : x[date_index], x[location_index], location_addition_index != -1 ? x[location_addition_index] : "", pangolin_index != -1 ? x[pangolin_index] : "", aasub_index == -1 ? "" : x[aasub_index], submission_date_index != -1 ? x[submission_date_index] : "");
                line = "";

                // if (x[id_index] == "EPI_ISL_15455320") {
                //     cerr << "MR EPI_ISL_15455320:" << x << " x[id_index]=" << x[id_index] << " metadata.id=" << metadata.id << "|" << endl;
                // }

                return true;
            }
        }
        return false;
    }
};
