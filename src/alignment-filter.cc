#define BOOST_BIND_GLOBAL_PLACEHOLDERS
#include <iostream>
#include <fstream>
#include <string>
#include <boost/iostreams/filtering_streambuf.hpp>
#include <boost/iostreams/copy.hpp>
#include <boost/iostreams/filter/gzip.hpp>
#include <boost/iostreams/filter/lzma.hpp>
#include <vector>
#include <set>
#include "tree.h"
#include "fasta.h"
#include <codecvt>
#include <ranges>


namespace io = boost::iostreams;
using namespace std;

struct FastaLoaderSequenceFilterIdIndex {
	int dup_count=0;
	std::set<std::string> samples_found;

	std::ostream& os;
	const std::set<std::string>& samples;

	int id_index;
	bool progress;

	FastaLoaderSequenceFilterIdIndex(std::ostream& os, const std::set<std::string>& samples, int id_index, bool progress = false) : os(os), samples(samples), id_index(id_index), progress(progress) {
	}

	void clear() {
		dup_count = 0;
		samples_found.clear();
	}

	std::string id(std::string raw_id) {
		vector<string> x = split(raw_id, '|');
		// if (raw_id.find("EPI_ISL_1001837") != string::npos) {
		// 	cerr << "D raw_id Found! " << x << " " << id_index << endl;
		// }
		if (id_index == -1) {
			for (const auto& xx : x) {
				if (xx.starts_with("EPI_ISL_")) {
					// if (xx == "EPI_ISL_1001837") {
					// 	cerr << "D Found! " << x << " " << xx << endl;
					// }
					return xx;
				}
			}
			if (!raw_id.starts_with("EPI_ISL_")) {
				cerr << "W invalid id: " << raw_id << endl; 
			}
			return raw_id;
		}
		if ((int)x.size() <= id_index) {
			cerr << "W " << raw_id << " short length " << id_index << endl;
			return raw_id;
		}
		return x[id_index];

		//id = x[id_index];
		//
		//if (raw_id.find("|") != std::string::npos) 
		//	raw_id = raw_id.substr(0, raw_id.find("|"));
		//return raw_id;
	}

	int count =0, accept_count=0;
	void process(std::string id, std::string seq) {
		if (count % 100000 == 0) {
			cerr << "\r" << accept_count << "/" << count << "          ";
		}
		count++;
		if (samples.find(id) != samples.end()) {
			if (samples_found.find(id) != samples_found.end()) {
				//cerr << "dup " << id << endl;
				dup_count++;
			} else {
			//cerr << "S " << id << endl;
				accept_count++;
				os << ">" << id << std::endl;
				os << seq << std::endl;
				samples_found.insert(id);
			}
		}
	}
};

// resolve<boost::iostreams::detail::chain_client<boost::iostreams::chain<boost::iostreams::input, char, std::char_traits<char>, std::allocator<char> > >::mode, boost::iostreams::detail::chain_client<boost::iostreams::chain<boost::iostreams::input, char, std::char_traits<char>, std::allocator<char> > >::char_type>(std::basic_istream<wchar_t>&) {

// }

#include <boost/iostreams/traits.hpp>
#include <boost/iostreams/read.hpp>
#include <boost/iostreams/write.hpp>
#include <boost/iostreams/seek.hpp>

// CategoryT: boost.iostreams device category tag
// DeviceT  : type of associated narrow device
// CharT    : (wide) character type of this device adapter 
template< typename CategoryT, typename DeviceT, typename CharT >
class basic_reinterpret_device
{
public:
    using category = CategoryT;               // required by boost::iostreams device concept
    using char_type = CharT;                  // required by boost::iostreams device concept
    using associated_device = DeviceT;
    using associated_char_type = typename boost::iostreams::char_type_of< DeviceT >::type;
    static_assert( sizeof( associated_char_type ) == 1, "Associated device must have a byte-sized char_type" );

    // Default constructor.
    basic_reinterpret_device() = default;

    // Construct from a narrow device
    explicit basic_reinterpret_device( DeviceT* pDevice ) :
        m_pDevice( pDevice ) {}

    // Get the asociated device.
    DeviceT* get_device() const { return m_pDevice; }

    // Read up to n characters from the underlying data source into the buffer s, 
    // returning the number of characters read; return -1 to indicate EOF
    std::streamsize read( char_type* s, std::streamsize n )
    {
        ThrowIfDeviceNull();

        std::streamsize bytesRead = boost::iostreams::read( 
            *m_pDevice, 
            reinterpret_cast<associated_char_type*>( s ), 
            n * sizeof( char_type ) );

        if( bytesRead == static_cast<std::streamsize>( -1 ) )  // EOF
            return bytesRead;
        return bytesRead / sizeof( char_type );
    }

    // Write up to n characters from the buffer s to the output sequence, returning the 
    // number of characters written.
    std::streamsize write( const char_type* s, std::streamsize n )
    {
        ThrowIfDeviceNull();

        std::streamsize bytesWritten = boost::iostreams::write(
            *m_pDevice, 
            reinterpret_cast<const associated_char_type*>( s ), 
            n * sizeof( char_type ) );

        return bytesWritten / sizeof( char_type );
    }

    // Advances the read/write head by off characters, returning the new position, 
    // where the offset is calculated from:
    //  - the start of the sequence if way == ios_base::beg
    //  - the current position if way == ios_base::cur
    //  - the end of the sequence if way == ios_base::end
    std::streampos seek( std::streamoff off, std::ios_base::seekdir way ) 
    {
        ThrowIfDeviceNull();

        std::streampos newPos = boost::iostreams::seek( *m_pDevice, off * sizeof( char_type ), way );
        return newPos / sizeof( char_type );
    }

protected:
    void ThrowIfDeviceNull()
    {
        if( ! m_pDevice )
            throw std::runtime_error( "basic_reinterpret_device - no associated device" );
    }

private:
    DeviceT* m_pDevice = nullptr;
};


#include <boost/iostreams/categories.hpp>
#include <boost/iostreams/traits.hpp>
#include <boost/iostreams/stream.hpp>
#include <boost/iostreams/stream_buffer.hpp>

struct reinterpret_device_tag : virtual boost::iostreams::source_tag, virtual boost::iostreams::sink_tag {};
struct reinterpret_source_seekable_tag : boost::iostreams::device_tag, boost::iostreams::input_seekable {};
struct reinterpret_sink_seekable_tag : boost::iostreams::device_tag, boost::iostreams::output_seekable {};


template< typename DeviceT, typename CharT >
using reinterpret_source = basic_reinterpret_device< boost::iostreams::source_tag, DeviceT, CharT >;

template< typename DeviceT, typename CharT >
using reinterpret_sink = basic_reinterpret_device< boost::iostreams::sink_tag, DeviceT, CharT >;

template< typename DeviceT, typename CharT >
using reinterpret_device = basic_reinterpret_device< reinterpret_device_tag, DeviceT, CharT >;

template< typename DeviceT, typename CharT >
using reinterpret_device_seekable = basic_reinterpret_device< boost::iostreams::seekable_device_tag, DeviceT, CharT >;

template< typename DeviceT, typename CharT >
using reinterpret_source_seekable = 
    basic_reinterpret_device< reinterpret_source_seekable_tag, DeviceT, CharT >;

template< typename DeviceT, typename CharT >
using reinterpret_sink_seekable = 
    basic_reinterpret_device< reinterpret_sink_seekable_tag, DeviceT, CharT >;


template< typename DeviceT >
using reinterpret_as_wistreambuf = boost::iostreams::stream_buffer< reinterpret_source_seekable< DeviceT, wchar_t > >;

template< typename DeviceT >
using reinterpret_as_wostreambuf = boost::iostreams::stream_buffer< reinterpret_sink_seekable< DeviceT, wchar_t > >;

template< typename DeviceT >
using reinterpret_as_wstreambuf = boost::iostreams::stream_buffer< reinterpret_device_seekable< DeviceT, wchar_t > >;


template< typename DeviceT >
using reinterpret_as_wistream = boost::iostreams::stream< reinterpret_source_seekable< DeviceT, wchar_t > >;

template< typename DeviceT >
using reinterpret_as_wostream = boost::iostreams::stream< reinterpret_sink_seekable< DeviceT, wchar_t > >;

template< typename DeviceT >
using reinterpret_as_wstream = boost::iostreams::stream< reinterpret_device_seekable< DeviceT, wchar_t > >;
// template<>
// class my_wistream_reinterpreter : public wistream<> {

// };

int main(int argc, char* argv[]) {
	//cerr << "main" << endl;
	// wifstream file(argv[1], ios_base::in | ios_base::binary);
	// wifstream file(argv[1]);
	// cerr << "file opened " << argv[1] << endl;
	// // io::filtering_streambuf<io::input> in;
	// // //in.push(gzip_decompressor());
	// // in.push(io::lzma_decompressor());
	// // in.push(file);
	// // std::istream incoming(&in);
	// // std::wistream my_wistream_reinterpreter(incoming);
	// // reinterpret_as_wistream< std::istream > my_wistream_reinterpreter(incoming);

	// // wistream& file_alignment = endswith(argv[1],".xz") ? my_wistream_reinterpreter : file; 
	// if (endswith(argv[1],".xz")) {
	// 	cerr << "We don't support xz file " << argv[1] << endl;
	// }
	// assert(!endswith(argv[1],".xz"));
	// wistream& file_alignment = file; 
	ifstream file_alignment(argv[1]);

	vector<string> sample_ids;
	ifstream fi(argv[2]);
	for (string line; getline(fi, line); ) {
		if (line != "") {
			sample_ids.push_back(line);
			//cerr << "id=(" << line << ")" << endl;
		}
	}
	cerr << "sample_ids loaded " << sample_ids.size() << " id-index=" << (argc > 3 ? std::atoi(argv[3])-1 : 0) << endl;
	for (int i=0; i<(int)sample_ids.size() && i <= 5; i++) {
		cerr << "'" << sample_ids[i] << "' ";
	}
	cerr << endl;

	set<string> samples(sample_ids.begin(), sample_ids.end()), samples_found;
	FastaLoaderSequenceFilterIdIndex filter(cout, samples, argc > 3 ? std::atoi(argv[3])-1 : 0, true);
	FastaLoader<FastaLoaderSequenceFilterIdIndex> fastaLoader(filter);
	fastaLoader.load(file_alignment);
	cerr << endl;
	cerr << " info: lines="  << fastaLoader.line_count << " records=" << fastaLoader.record_count << endl;

	cerr << "saved " << fastaLoader.filter.samples_found.size() << " samples " << endl;
	cerr << "  duplicate samples in fasta file: " << fastaLoader.filter.dup_count << endl;
	cerr << "  not found samples in fasta file: ";
	int samples_notfount_count = 0;
	for (auto const& s:samples) {
		if (fastaLoader.filter.samples_found.find(s) == fastaLoader.filter.samples_found.end()) {
			if (samples_notfount_count < 10)
				cerr <<  s << " ";
			samples_notfount_count++;
		}
	}
	if (samples_notfount_count > 0) {
		cerr << " (" << samples_notfount_count << " samples)" << endl;
	}


	return 0;
}
