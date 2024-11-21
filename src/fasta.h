#include <istream>
#include <string>
#include <map>
#include <set>
#include <cassert>
#include <ranges>
#include <locale>  // consume_header, locale
#include <codecvt> // codecvt_utf8_utf16

using namespace std;

template<typename FILTER>
struct FastaLoader {

	FILTER filter;
	FastaLoader(const FILTER& filter = FILTER()) : filter(filter) {
	}

	std::string to_string(const std::string& s) const {
		return s;
	}

	std::string to_string(const std::wstring& s) const {
		
		auto str_v = s | std::views::filter([] (wchar_t c) { return ((uint)c) < 128;})  | std::views::transform([] (wchar_t c) { return (char) c;});
		std::string str(str_v.begin(), str_v.end());

		// wstring wstr(str.begin(), str.end());

		// if (str.size() != s.size()) {
		// 	wcerr << L"W string with unicode: " << str.size() << L" " << s.size() << L" " << s << L" " << wstr << endl;
		// }

		return str;
	}

	int line_count = 0, record_count = 0;

	template<typename STREAM = std::istream>
	void load(STREAM& file_alignment) {
		line_count = 0;
		record_count = 0;

	    // std::locale loc("en_US.UTF-8"); // You can also use "" for the default system locale
	    // file_alignment.imbue(loc); // Use it for file input
		// std::locale loc("C.UTF-8");
		// const std::locale loc = std::locale(std::locale(), new std::codecvt_utf8<wchar_t>());
		// file_alignment.imbue(loc);

		// file_alignment.imbue(std::locale(
		// 	file_alignment.getloc(),
		// 	new std::codecvt_utf8_utf16<wchar_t, 0x10FFFF, std::consume_header>));

		// std::cerr << "Stream is in a fail=" << file_alignment.fail() << " bad=" << file_alignment.bad() << " good=" << file_alignment.good() << " eof=" << file_alignment.eof()<< " rdstate=" << file_alignment.rdstate() << " state." << std::endl;

		// std::cerr << "LOAD: " << filter.file_size << " " << std::endl;
		filter.clear();
		std::string seq, id, raw_id;
		for (std::string line; getline(file_alignment, line); ) {
			//cerr << "L " << line << endl;
			// if (wline.find(L"EPI_ISL_1001837") != wstring::npos) {
			// 	cerr << "D Found EPI_ISL_1001837! " << to_string(wline) << endl;
			// }
			// cerr << "L '" << to_string(wline) << "'" << endl;
			// std::cerr << "Stream is in a fail=" << file_alignment.fail() << " bad=" << file_alignment.bad() << " good=" << file_alignment.good() << " eof=" << file_alignment.eof()<< " rdstate=" << file_alignment.rdstate() << " state." << std::endl;

			// string line = to_string(wline);
			// if (line.find("EPI_ISL_1001837") != string::npos) {
			// 	cerr << "D Found EPI_ISL_1001837! S: " << line << endl;
			// }
			if (line.size() == 0) continue;
			if (line[0] == '>') {
				if (id.size() > 0) {
					filter.process(id, seq, raw_id, file_alignment);
					record_count++;
				}
				//id = split(line.substr(1), '|')[0];
				id = line.substr(1);
				raw_id = id;
				id = filter.id(id);
				seq = "";
				//cerr << "new id (" << id << ") " << id.size() << endl;
			} else {
				if (seq.size() == 0)
					seq = line;
				else
					seq += line;
			}
			// if (line_count % 100000 == 0) {
			// 	filter.process(id, seq, raw_id, file_alignment);
			// }
			line_count++;

			//TODO: DEBUG
			// if (line_count > 100000000) {
			// 	break;
			// }
		}
		if (id != "") {
			filter.process(id, seq, raw_id, file_alignment);
			record_count++;
		}


		// std::cerr << "Stream is in a fail=" << file_alignment.fail() << " bad=" << file_alignment.bad() << " good=" << file_alignment.good() << " eof=" << file_alignment.eof()<< " rdstate=" << file_alignment.rdstate() << " state." << std::endl;

	}


	void load(std::string fn) {
		std::wifstream fi(fn);
		load(fi);
	}
};

struct FastaLoaderSequenceFilter {
	int dup_count=0;
	std::set<std::string> samples_found;

	std::ostream& os;
	const std::set<std::string>& samples;

	FastaLoaderSequenceFilter(std::ostream& os, const std::set<std::string>& samples) : os(os), samples(samples) {
	}

	void clear() {
		dup_count = 0;
		samples_found.clear();
	}

	std::string id(std::string raw_id) {
		if (raw_id.find("|") != std::string::npos) 
			raw_id = raw_id.substr(0, raw_id.find("|"));
		return raw_id;
	}

	void process(std::string id, std::string seq) {
		if (samples.find(id) != samples.end()) {
			if (samples_found.find(id) != samples_found.end()) {
				//cerr << "dup " << id << endl;
				dup_count++;
			} else {
			//cerr << "S " << id << endl;
				os << ">" << id << std::endl;
				os << seq << std::endl;
				samples_found.insert(id);
			}
		}
	}
};

struct FastaLoaderSequenceRename {
	int dup_count=0;
	std::set<std::string> samples_found;

	std::ostream& os;
	const std::map<std::string, std::string>& name_to_id;

	FastaLoaderSequenceRename(std::ostream& os, const std::map<std::string, std::string>& name_to_id) : os(os), name_to_id(name_to_id) {
	}

	void clear() {
		dup_count = 0;
		samples_found.clear();
	}

	std::string id(std::string raw_id) {
		return raw_id;
	}

	void process(std::string id, std::string seq) {
		auto i = name_to_id.find(id);
		if (i == name_to_id.end()) {
			std::cerr << "name not in metadata " << id << std::endl;
		}
		assert(i != name_to_id.end());
		os << ">" << i->second << std::endl;
		os << seq << std::endl;
		samples_found.insert(id);
	}
};

