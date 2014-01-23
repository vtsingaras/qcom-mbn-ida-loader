#include <windows.h>
#include <vector>
#include <..\ldr\idaldr.h>

typedef std::vector<unsigned char> CHARS;
#define SBL_HEADER_SIZE sizeof(SBL_HEADER)
const char BOOT_SEGMENT[] = "QCOM_BOOT";

struct SBL_HEADER {
	int load_index;
	int flash_partition_version; 	// 3 = nand
	int image_source_pointer;	// This + 40 is the start of the code in the file
	int image_dest_pointer;	// Where it's loaded in memory
	int image_size;               // code_size + signature_size + cert_chain_size
	int code_size;	        // Only what's loaded to memory
	int signature_addr;
	int signature_size;
	int cert_chain_addr; 		// Max of 3 certs?
	int cert_chain_size;
};

bool LoadFile(linput_t * file, CHARS & data)
{
	unsigned size = qlsize(file);
	data.resize(size);

	if (size > 0)
	{
		if (qlread(file, &data[0], size) == -1)
		{
			data.resize(0);
		}
	}
	return data.size() == size;
}

bool GetStageHeader(const unsigned char* p, unsigned size, SBL_HEADER& stgh)
{
	if (size < 0x100) return false;

	unsigned int* dw = (unsigned int*)p;
	stgh.load_index = dw[0];
	stgh.flash_partition_version = dw[1];
	stgh.image_source_pointer = dw[2];
	stgh.image_dest_pointer = dw[3];
	stgh.image_size = dw[4];
	stgh.code_size = dw[5];
	stgh.signature_addr = dw[6];
	stgh.signature_size = dw[7];
	stgh.cert_chain_addr = dw[8];
	stgh.cert_chain_size = dw[9];


	/*static const unsigned char FWEND[] = { 0xFF, 0xFF, 0x5A, 0xA5 };
	unsigned char* pend = memsrch_r((unsigned char*)p, size, (unsigned char*)FWEND, sizeof(FWEND));
	if (!pend) return false;
	pend += 2;

	unsigned* dw = (unsigned*)p;

	switch (dw[0])
	{
	case 0xE59FF008:
		fwi.base = dw[4] - 0x14;
		break;
	case 0xE59FF00C:
		fwi.base = dw[5] - 0x18;
		break;
	case 0xEA000002:
	case 0xEA000003:
	default:
		fwi.base = dw[1] - (pend - p);
	}

	if ((fwi.base % 4) != 0) return false; //must be aligned
	if (fwi.base + size < fwi.base) return false; //overflow
	if ((dw[2] % 2) != 0 || dw[2] - fwi.base >= pend - p - 3)  return false; //align & bounds
	if ((dw[3] % 2) != 0 || dw[3] - fwi.base >= pend - p - 3)  return false;
	fwi.hwid = *(unsigned short*)(p + dw[2] - fwi.base);
	fwi.swvr = *(unsigned short*)(p + dw[3] - fwi.base);*/
	return true;
}

int  idaapi accept_file(linput_t * file, char fileformatname[MAX_FILE_FORMAT_NAME], int n)
{
	if (n>0) return 0;

	SBL_HEADER stgh;
	CHARS data;
	if (!LoadFile(file, data) || data.size() == 0 || !GetStageHeader(&data[0], data.size(), stgh))
	{
		return 0;
	}

	qsnprintf(fileformatname, MAX_FILE_FORMAT_NAME, "Qualcomm MBN Bootchain Stage");

	set_processor_type("ARM", SETPROC_ALL);

	return 1;
}

void idaapi load_file(linput_t * file, ushort neflags, const char * formatname)
{
	SBL_HEADER stgh;
	CHARS data;

	if (!LoadFile(file, data) || data.size() == 0)
	{
		loader_failure("cannot read input file\n");
	}

	if (!GetStageHeader(&data[0], data.size(), stgh))
	{
		loader_failure("input file has unknown format\n");
	}

	file2base(file, SBL_HEADER_SIZE, stgh.image_dest_pointer, stgh.image_dest_pointer + stgh.code_size, true);

	if (!add_segm(0, stgh.image_dest_pointer, stgh.image_dest_pointer + stgh.code_size, BOOT_SEGMENT, CLASS_CODE))
	{
		loader_failure("cannot create code segment\n");
	}

	segment_t *s = get_segm_by_name(BOOT_SEGMENT);
	set_segm_addressing(s, 1);
	add_entry(stgh.image_dest_pointer, stgh.image_dest_pointer, "boot_base", true);
	return;
}

int  idaapi save_file(FILE * file, const char * formatname)
{
	if (file == NULL) return 1;

	segment_t *s = get_segm_by_name(BOOT_SEGMENT);
	if (!s) return 0;

	base2file(file, 0, s->startEA, s->endEA);
	return 1;
}

__declspec(dllexport)
loader_t LDSC =
{
	IDP_INTERFACE_VERSION,
	0,
	accept_file,
	load_file,
	save_file,
	NULL,
	NULL,
};