#include <string.h>
#include <errno.h>

#include "fitsfile.h"
#include "fitsioutils.h"

void fitsfile_extension_close(fitsextension_t* ext) {
    if (!ext)
        return;
    if (ext->header)
        qfits_header_destroy(ext->header);
    //free(ext);
}

void fitsfile_extension_set_header(fitsextension_t* ext,
                                   qfits_header* hdr) {
    ext->header = hdr;
}

qfits_header* fitsfile_extension_get_header(fitsextension_t* ext) {
    return ext->header;
}

int fitsfile_extension_write_header(fitsextension_t* ext, FILE* fid) {
	ext->header_start = ftello(fid);
    if (qfits_header_dump(ext->header, fid))
		return -1;
    ext->header_end = ftello(fid);
    return 0;
}

int fitsfile_extension_fix_header(fitsextension_t* ext, FILE* fid) {
	off_t offset;
	off_t new_header_end;
	off_t old_header_end;
	offset = ftello(fid);
	fseeko(fid, ext->header_start, SEEK_SET);
    old_header_end = ext->header_end;
    if (fitsfile_extension_write_header(ext, fid))
        return -1;
	new_header_end = ext->header_end;

	if (new_header_end != old_header_end) {
		fprintf(stderr, "Error: header used to end at %lu, "
				"now it ends at %lu.  Data loss is likely!\n",
                (unsigned long)old_header_end, (unsigned long)new_header_end);
		return -1;
	}
	fseek(fid, offset, SEEK_SET);
	return 0;
}




char* fitsfile_filename(fitsfile_t* ff) {
    return ff->filename;
}

fitsfile_t* fitsfile_open_for_writing(const char* fn) {
	fitsfile_t* ff;
	ff = calloc(1, sizeof(fitsfile_t));
	if (!ff)
		return NULL;
	ff->filename = strdup(fn);

	ff->fid = fopen(ff->filename, "wb");
	if (!ff->fid) {
		fprintf(stderr, "Couldn't open file \"%s\" for output: %s\n", ff->filename, strerror(errno));
        free(ff->filename);
        free(ff);
        return NULL;
	}
    // the primary header
    ff->primary.header = qfits_header_default();
    ff->primary.header_start = 0;
    ff->primary.header_end = 0;
    return ff;
}

fitsfile_t* fitsfile_open(const char* fn) {
	fitsfile_t* ff;
	ff = calloc(1, sizeof(fitsfile_t));
	if (!ff)
		return NULL;
	ff->filename = strdup(fn);

    /*
     ff->fid = fopen(ff->filename, "rb");
     if (!ff->fid) {
     fprintf(stderr, "Couldn't open file \"%s\" for output: %s\n", ff->filename, strerror(errno));
     free(ff->filename);
     free(ff);
     return NULL;
     }
     */
    // the primary header
    ff->primary.header = qfits_header_read(fn);
    ff->primary.header_start = 0;
    ff->primary.header_end = 0;
    return ff;
}

int fitsfile_close(fitsfile_t* ff) {
    int rtn = 0;
	if (ff->fid) {
		fits_pad_file(ff->fid);
		if (fclose(ff->fid)) {
			fprintf(stderr, "Error closing fitsfile: %s\n", strerror(errno));
            rtn = -1;
        }
    }
	//qfits_header_destroy(ff->primheader);
    fitsfile_extension_close(&(ff->primary));
	free(ff->filename);
    free(ff);
    return rtn;
}

qfits_header* fitsfile_get_primary_header(fitsfile_t* ff) {
    return ff->primary.header;
}

int fitsfile_write_primary_header(fitsfile_t* ff) {
	if (qfits_header_dump(ff->primary.header, ff->fid))
		return -1;
	ff->primary.header_end = ftello(ff->fid);
	return 0;
}

int fitsfile_fix_primary_header(fitsfile_t* ff) {
	off_t offset;
	off_t new_header_end;
	off_t old_header_end;

	offset = ftello(ff->fid);
	fseeko(ff->fid, 0, SEEK_SET);
    old_header_end = ff->primary.header_end;

    if (fitsfile_write_primary_header(ff))
        return -1;
	new_header_end = ff->primary.header_end;

	if (new_header_end != old_header_end) {
		fprintf(stderr, "Error: header used to end at %lu, "
				"now it ends at %lu.  Data loss is likely!\n",
                (unsigned long)old_header_end, (unsigned long)new_header_end);
		return -1;
	}
	fseek(ff->fid, offset, SEEK_SET);
	return 0;
}


