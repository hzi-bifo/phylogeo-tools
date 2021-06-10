#include "tree.h"
#include "state.h"
#include <boost/program_options.hpp>
#include "rangetree.h"
#include <vector>
#include <numeric>

namespace po = boost::program_options;

array<string, StateInOut::size> StateInOut::names = {"uk", "nonuk"};


int days_since(const string& date, int y = 1900, int m = 1, int d = 1) {
	tm then = {0};
	then.tm_year = y - 1900;
	then.tm_mon = m - 1;
	then.tm_mday = d;
	time_t then_secs = mktime(&then);

	tm date_tm = {0};
	if (!strptime(date.c_str(), "%Y-%m-%d", &date_tm)) {
		throw exception();
	}
	//cerr << "D " << date_tm.tm_year << " " << date_tm.tm_mon << " " << date_tm.tm_mday << " " << date_tm.tm_hour << " " << endl;
	//cerr << "D " << then.tm_year << " " << then.tm_mon << " " << then.tm_mday << " " << then.tm_hour << " " << endl;
	time_t t = mktime(&date_tm);  // t is now your desired time_t

	double days = difftime(t, then_secs);
	//cerr << "days_since_1900 " << date << " " << days << endl;
	return ((long)days) / 24 / 60 / 60;
}       

bool operator<(const StateInOut& a, const StateInOut& b) {
	return a.type < b.type;
}


struct Data;
typedef Node<StateInOut, Data> INode;

struct Data {
	bool sampled;
	int dfs_num;
	int date;
	Data() : sampled(false), dfs_num(-1), date(-1) {
	}
};

ostream& operator<<(ostream& os, const Data& d) {
	//os << "(" << d.dfs_vis <<"/" << d.dfs_fin << " " << d.depth << " ";
	//for (auto& p : d.parents)
	//	os << p->label << " ";
	//os << d.back_bone << " " << d.sampled << ")";
	return os;
}

struct Bucket {
	int epiweek;
	string country;

	Bucket(int epiweek=0, string country = "") : epiweek(epiweek), country(country) {
	}

	bool operator<(const Bucket& b) const {
		if (epiweek != b.epiweek) return epiweek < b.epiweek;
		return country < b.country;
	}
};

struct INodeStarCmpEarly {
	bool operator()(const INode* a, const INode* b) const {
		if (a->data.date != b->data.date) return a->data.date < b->data.date;
		return a->data.dfs_num < b->data.dfs_num;
	}
};
struct INodeStarCmpLate {
	bool operator()(const INode* a, const INode* b) const {
		if (a->data.date != b->data.date) return a->data.date > b->data.date;
		return a->data.dfs_num < b->data.dfs_num;
	}
};

//we assume branch_length represets dissimilarity
struct Sampler {
	const map<string, Metadata>& metadata;
	int bucket_sample_size_in, bucket_sample_size_out;
	int removed_internal_count, removed_sample_count;

	size_t special_count = 0;
	set<string> special_nodes;

	size_t keep_count = 0, keep_max = 0;
	set<string> keep_nodes;

	Sampler(const map<string, Metadata>& metadata, int bucket_sample_size_in = 0, int bucket_sample_size_out=0) : metadata(metadata), bucket_sample_size_in(bucket_sample_size_in), bucket_sample_size_out(bucket_sample_size_out), removed_internal_count(0), removed_sample_count(0) {
	}

	map<Bucket, vector<INode*>> buckets;

	Bucket calc_bucket(const INode& n) {
		auto m = metadata.find(n.label);
		assert(m != metadata.end());
		vector<string> loc_split = split(m->second.location, '/');
		return Bucket(days_since(m->second.date) / 7, trim(loc_split[1]));
		//return Bucket(days_since(m->second.date) / 7, n.location);
	}

	void put_to_bucket(INode& n, int& dfs_num) {
		n.data.dfs_num = dfs_num++;
		if (n.isLeaf()) {
			buckets[calc_bucket(n)].push_back(&n);
			auto m = metadata.find(n.label);
			n.data.date = days_since(m->second.date);
		}
		for (auto & c : n.children) {
			put_to_bucket(c, dfs_num);
		}
	}

	// main entry
	INode run(INode& n) {
		int dfs_num = 0;
		put_to_bucket(n, dfs_num);
		sample(bucket_sample_size_in, bucket_sample_size_out);
		if (special_count > 0)
			sample_special(n);
		//we add samples from keep_nodes to have about keep_count samples
		if (keep_count > 0) {
			sample_keep(n);
		}
		return sample(n).first;
	}

	void sample_keep(INode& n) {
		if (keep_nodes.find(n.label) != keep_nodes.end()) {
			int dfs_num = 0;
			buckets.clear();
			put_to_bucket(n, dfs_num);
			size_t sampled_size = 0;
			vector<INode*> all;
			vector<INode*> sampled;
			for (auto & b : buckets) {
				all.insert(all.end(), b.second.begin(), b.second.end());
				for (auto const & inp : b.second) {
					if (inp->data.sampled) {
						sampled_size++;
						sampled.push_back(&*inp);
					}
				}
			}
			size_t sampled_size_old = sampled_size;
			if (sampled_size < keep_count) {
				random_shuffle(all.begin(), all.end());
				for (size_t i=0; i < all.size() && sampled_size < keep_count; i++) {
					if (all[i]->data.sampled == false) {
						all[i]->data.sampled = true;
						sampled_size++;
					}
				}
				cerr << "K+ " << n.label << " " << sampled_size << " (" << sampled_size_old << ")" << endl;
			} else if (sampled_size > keep_max && keep_max > 0) {
				random_shuffle(sampled.begin(), sampled.end());
				for (auto i = sampled.begin() + keep_max; i != sampled.end(); i++) {
					(*i)->data.sampled = false;
					sampled_size--;
				}
				cerr << "K- " << n.label << " " << sampled_size << " (" << sampled_size_old << ")" << endl;
			} else {
				cerr << "k " << n.label << " " << sampled_size << " keep-count:(" << keep_count << ")" << endl;
			}
		}
		for (auto & c : n.children)
			sample_keep(c);
	}

	template<typename T, typename C>
	void set_add_and_keep(set<T,C>& a, set<T,C>& b, size_t k) {
		a.insert(b.begin(), b.end());
		if (a.size() > k) {
			//next could be implemented faster, here since k is small, it should not make any problem
			a.erase(std::next(a.begin(), k), a.end());
		}
	}

	pair<set<INode*, INodeStarCmpEarly>, set<INode*, INodeStarCmpLate>> sample_special(INode& n) {
		pair<set<INode*, INodeStarCmpEarly>, set<INode*, INodeStarCmpLate>> r;
		if (n.isLeaf()) {
			r.first.insert(&n);
			r.second.insert(&n);
			//cerr << "L " << n.label << endl;
		}
		for (auto & c : n.children) {
			pair<set<INode*, INodeStarCmpEarly>, set<INode*, INodeStarCmpLate>> c_r = sample_special(c);
			set_add_and_keep(r.first, c_r.first, special_count);
			set_add_and_keep(r.second, c_r.second, special_count);
		}
		if (special_nodes.find(n.label) != special_nodes.end()) {
			cerr << "Sp " << n.label << " " << n.height << " " << endl;
			for (auto & l : r.first) {
				//cerr << " early " << l->label << " " << l->data.date << " " << l->data.dfs_num << " " << l->location << endl;
				l->data.sampled = true;
			}
			for (auto & l : r.second) {
				//cerr << " late " << l->label << " " << l->data.date << " " << l->data.dfs_num << " " << l->location << endl;
				l->data.sampled = true;
			}
		}
		return r;
	}

	//sample from each bucket, it does not count already sampled ones
	void sample(int sample_size_in, int sample_size_out) {
		cerr << "sampling from " << buckets.size() << " buckets" << endl;
		for (auto & b : buckets) {
			if (b.first.country == "") continue;
			random_shuffle(b.second.begin(), b.second.end());
			//int size = b.first.country == StateInOut::IN ? bucket_sample_size_in : bucket_sample_size_out;
			StateInOut locationInOut = b.first.country == StateInOut::names[0] ? StateInOut::IN : StateInOut::OUT ;
			int size = locationInOut == StateInOut::IN ? sample_size_in : sample_size_out;
			int size_0 = size;
			for (int i=0; size > 0 && i < (int) b.second.size(); i++) {
				if (b.second[i]->data.sampled == false) {
					b.second[i]->data.sampled = true;
					size--;
				}
			}
			cerr << "  b " << b.first.country << " " << b.first.epiweek << " s=" << (size_0 - size) << " -- ";
		}
		cerr << endl;
	}

	//rebuild the tree based on n.data.sampled for all nodes n
	pair<INode, bool> sample(const INode& n) {
		//height, size should be recalculated
		INode r(n.label, n.branch_length, n.annotation, vector<INode>(), n.location, 1, 1, n.isLeaf() ? 1 : 0);
		for (auto &c: n.children) {
			pair<INode, bool> c_c_ = sample(c);
			if (c_c_.second == false) {
				if (c.isLeaf())
					removed_sample_count++;
				else
					removed_internal_count++;
				continue;
			}
			INode& c_c = c_c_.first;
			if (c_c.children.size() == 1 && !c.isLeaf()) {
				//cerr << "node removed " << c.label << " " << c.branch_length << endl;
				removed_internal_count++;
				for (auto& cc: c_c.children) {
					cc.branch_length += c.branch_length;
					r.height = max(r.height, cc.height + 1);
					r.size += cc.size;
					//r.children.push_back(move(cc));
					r.sample_size += cc.sample_size;
					r.children.push_back(cc);
				}
			} else {
				//normal:
				r.height = max(r.height, c_c.height + 1);
				r.size += c_c.size;
				r.sample_size += c_c.sample_size;
				//r.children.push_back(move(c_c));
				r.children.push_back(c_c);
			}
		}
		return make_pair(r, r.children.size() > 0 || n.data.sampled == true);
	}

	int sample_in = 0, sample_out = 0, all_in=0, all_out=0;
	void write_sample_file(ostream& fo, const INode& n) {
		if (n.isLeaf()) {
			if (n.location == StateInOut::IN)
				sample_in++;
			else
				sample_out++;
			fo << n.label << endl;
		}
		if (n.location == StateInOut::IN)
			all_in++;
		else
			all_out++;
		for (auto const& c: n.children) {
			write_sample_file(fo, c);
		}
	}

	void write_sample_file(string fn, const INode& n) {
		sample_in = 0, sample_out = 0, all_in=0, all_out=0;
		ofstream fo(fn);
		write_sample_file(fo, n);
		cerr << "Samples in=" << sample_in << "/" << all_in << " out=" << sample_out << "/" << all_out << endl;
	}

	/*

	RangeTree seg;
	vector<INode*> samples;
	vector<Point> node_dfs_values;

	int dfs_vis, dfs_fin;
	void dfs(INode& n, vector<INode*>& parents, double depth) {
		if (n.isLeaf()) {
			samples.push_back(&n);
		}
		n.data = Data(dfs_vis++, -1, depth + n.branch_length);
		for (size_t i=1; parents.size() >= i; i*=2) {
			n.data.parents.push_back(parents[parents.size() - i]);
		}
		//cerr << "D " << n.label << " " << parents.size() << " " << n.data.parents.size() <<  endl;
		parents.push_back(&n);
		for (auto &c: n.children) {
			dfs(c, parents, depth + n.branch_length);
		}
		parents.pop_back();
		n.data.dfs_fin = dfs_fin++;
		node_dfs_values.push_back(Point(n.data.dfs_vis, n.data.dfs_fin));
	}
	
	INode run(INode& n) {
		// we are more restricted for state samples
		assert(range_for_state <= range);
		dfs_vis = dfs_fin = 0;
		node_dfs_values.clear();
		vector<INode*> parents;
		dfs(n, parents, 0);
		cerr << "dfs called" << endl;
		seg = RangeTree(dfs_vis, dfs_fin, node_dfs_values);
		cerr << "seg created" << endl;

		sort(samples.begin(), samples.end(), [](INode* s1, INode* s2) {return s1->data.depth > s2->data.depth;});
		cerr << "samples sorted" << endl;

		for (auto& s: samples) {
			if (!is_covered(*s)) {
				s->data.sampled = true;
				add(*s);
			}
		}

		return sample(n).first;
	}

	void add(INode& n) {
		//cerr << "+ " << n.label << endl;
		INode* nn = &n;
		while (true) { 
			nn->data.back_bone = true;
			if (nn->data.parents.size() > 0) 
				nn = nn->data.parents[0];
			else
				break;
		}
		//cerr << "  seg. " << n.data.dfs_vis << " " << n.data.dfs_fin << "=" << n.data.depth << endl;
		seg.modify(n.data.dfs_vis, n.data.dfs_fin, n.data.depth);
	}

	bool is_covered(INode& n) {
		INode* p = &n;
		//cerr << " ? " << n.label << " " << n.data;
		for (int l = ((int)p->data.parents.size())-1; p->data.parents.size() > 0 && p->data.parents[0]->data.back_bone == false; ) {
			if (l < (int)p->data.parents.size() && p->data.parents[l]->data.back_bone == false)
				p = p->data.parents[l];
			else
				l--;
		}
		//cerr << " first-non-backbone:" << p->label;
		// selected.vis >= p->data.dfs_vis  && selected.fin < p->data.dfs_fin
		if (p->data.parents.size() > 0) {
			Data& data = p->data.parents[0]->data;
			double d = seg.query(data.dfs_vis, 0, dfs_vis, data.dfs_fin),
				min_dis_to_selected_samples = d - data.depth + n.data.depth - data.depth;
			//cerr << " r="<<(d - data.depth + n.data.depth - data.depth) << " bb:" << p->data.parents[0]->label << " d:" << d << " bb-d: " << data.depth << " my-d:" << n.data.depth << endl;
			if ((n.location == StateInOut::IN and min_dis_to_selected_samples <= range_for_state) or (n.location == StateInOut::OUT and min_dis_to_selected_samples <= range)) {
				//if (n.location == StateInOut::IN) {
				//	cerr << "W IN not selected " << n.location << " " << min_dis_to_selected_samples << " " << range_for_state << " " << range << " " << StateInOut::IN << endl;
				//}
				return true;
			} else {
				return false;
			}
		} else {
			//no sample selected yet
			//cerr << "  cov: first" << endl;
			return false;
		}
	}



	*/

};

// Algorithm: 
//   puts samples to buckets, sample evenly from each.
//   for special internal nodes, it keeps some amount of earliest and oldest samples.

int main(int argc, char* argv[]) {

	// Declare the supported options.
	po::options_description desc("Allowed options");
	desc.add_options()
	    ("help", "Run sankoff")
	    ("in", po::value<string>(), "input tree")
	    ("out", po::value<string>(), "output tree")
	    ("bucket-size", po::value<vector<int>>()->multitoken()->default_value(vector<int>{100, 100}, "100 100"), "sample size from each bucket (in,out)")
	    ("special", po::value<vector<string>>()->multitoken()->default_value(vector<string>{""}, ""), "special attention internal nodes: e.g. 5 inode0 inode25, means 5 earliest and oldest samples of sub-tree of inode0 and inode25")
	    ("keep", po::value<vector<string>>()->multitoken()->default_value(vector<string>{""}, ""), "list of subtrees for which at least some samples to be kept")
	    ("metadata", po::value<string>(), "metadata file")
	    ("samples-out", po::value<string>(), "output file to write sample ids")
	    ("location_label,l", po::value<vector<string>>()->multitoken(), "location labels, e.g. Germany nonGermany")
	    ("print-annotation", po::value<bool>()->default_value(true), "print annotation")
	    ("print-internal-node-label", po::value<bool>()->default_value(true), "print internal node labels")
	    ("ilabel", po::value<bool>()->default_value(false), "override internal node labels")
	;

	po::variables_map vm;
	po::store(po::parse_command_line(argc, argv, desc), vm);

	try {
		po::notify(vm);
	} catch (std::exception& e) {
		std::cerr << "Error: " << e.what() << "\n";
		return 1;
	}

	StateInOut::names = {vm["location_label"].as<vector<string>>()[0], vm["location_label"].as<vector<string>>()[1]};

	string tree_file_name = vm["in"].as<string>(),
		output = vm["out"].as<string>();
	INode phylo = load_tree<INode>(tree_file_name) ;
	ifstream metadata_file(vm["metadata"].as<string>());
	map<string, Metadata> metadata = load_map(metadata_file);

	Sampler sampler(metadata, vm["bucket-size"].as<vector<int>>()[0], vm["bucket-size"].as<vector<int>>()[1]);
	vector<string> special = vm["special"].as<vector<string>>();
	if (special.size() >= 2) {
		sampler.special_count = stoi(special[0]);
		sampler.special_nodes = set<string>(special.begin() + 1, special.end());
	}

	vector<string> keep = vm["keep"].as<vector<string>>();
	if (keep.size() >= 3) {
		sampler.keep_count = stoi(keep[0]);
		sampler.keep_max = stoi(keep[1]);
		sampler.keep_nodes = set<string>(keep.begin()+2, keep.end());
	} else {
		if (keep.size() > 0) {
			cerr << "Invalid number for keep" << endl;
			return 1;
		}
	}
	INode contracted_phylo = sampler.run(phylo);

	/*
	cerr << "buckets:" << endl;
	for (auto const& b : sampler.buckets) {
		cerr << "  " << b.first.epiweek << " " << b.first.country << "->" << b.second.size() << endl;
	}
	*/
	cerr << "removed " << sampler.removed_internal_count << " internal nodes and " << sampler.removed_sample_count << " samples " << endl;
	cerr << "  h=" << phylo.height << ">" << contracted_phylo.height << " s=" << phylo.size << ">" << contracted_phylo.size << " ss=" << phylo.sample_size << ">" << contracted_phylo.sample_size << endl;

	if (vm["ilabel"].as<bool>() == true) {
		InternalNodeLabeler<INode> internalNodeLabeler;
		internalNodeLabeler.run(contracted_phylo);
		cerr << "internal nodes relabeled" << endl;
	}
	
	ofstream fo(output);
	NodePrinterGeneral<INode> np(vm["print-annotation"].as<bool>(), vm["print-internal-node-label"].as<bool>());
	//np.print(fo, phylo) << ";" << endl;
	np.print(fo, contracted_phylo) << ";" << endl;

	if (vm.count("samples-out") > 0) {
		sampler.write_sample_file(vm["samples-out"].as<string>(), contracted_phylo);
	}

	cerr << "output saved on " << output<< " " << "[" << vm["print-annotation"].as<bool>() << " " << vm["print-internal-node-label"].as<bool>() << "]" << endl;
	return 0;
}
