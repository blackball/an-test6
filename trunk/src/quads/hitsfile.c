#include <math.h>
#include "hitsfile.h"

void hits_header_init(hits_header* h) {
	memset(h, 0, sizeof(hits_header));
}

void hits_write_header(FILE* fid, hits_header* h) {
	/* All output is in one giant python dictionary so it can be read into
	 * python simply via:
	 * >>> hits = eval(file('my_hits_file.hits').read())
	 * >>> hits['fields_to_solve']
	 * 23
	 * >>> 
	 * Since most of our non-C code is in Python, this is convienent. */
	fprintf(fid, "# This is a hits file which records the results from a\n");
	fprintf(fid, "# solvexy run. It is a valid python literal and is easily\n");
	fprintf(fid, "# loaded into python via \n");
	fprintf(fid, "# >>> results = eval(file('myfile.hits').read())\n");
	fprintf(fid, "# >>> results['fields_to_solve']\n");
	fprintf(fid, "# 23\n\n");
	fprintf(fid, "dict(\n");
	fprintf(fid, "# SOLVER PARAMS:\n");
	fprintf(fid, "min_matches_to_agree = %u,\n", h->min_matches_to_agree);
	fprintf(fid, "min_overlap = %f,\n", h->overlap_needed);
	fprintf(fid, "min_field_objs = %u,\n", h->field_objs_needed);
	fprintf(fid, "# in arcsec:\n");
	fprintf(fid, "agree_tol = %g,\n", h->agreetol);

	fprintf(fid, "############################################################\n");
	fprintf(fid, "# Result data, stored as a list of dictionaries\n");
	fprintf(fid, "results = [ \n");
}

void hits_write_tailer(FILE* fid) {
	fprintf(fid, "], \n");
	fprintf(fid, ") \n");
	fprintf(fid, "# END OF RESULTS\n");
	fprintf(fid, "############################################################\n");
}

void hits_field_init(hits_field* h) {
	memset(h, 0, sizeof(hits_field));
}

void hits_start_hits_list(FILE* fid) {
	fprintf(fid, "    quads=[\n");
}

void hits_end_hits_list(FILE* fid) {
	fprintf(fid, "    ],\n");
}

void hits_write_field_header(FILE* fid, hits_field* h) {
	fprintf(fid, "# --------------------\n");
	fprintf(fid, "dict(\n");
	if (h->failed)
		fprintf(fid, "    failed=True,\n");
	fprintf(fid, "    field=%u,\n", h->field);
	fprintf(fid, "    # Image corners\n");
}


void hits_write_field_tailer(FILE* fid) {
	fprintf(fid, "),\n");
}

void hits_write_hit(FILE* fid, MatchObj* mo) {
	if (!mo) {
		fprintf(fid, "        # No agreement between matches. Could not resolve field.\n");
		return;
	}
	fprintf(fid, "        dict(\n");

	fprintf(fid, "            index_id = %i,\n", (int)mo->indexid);
	fprintf(fid, "            healpix = %i,\n", (int)mo->healpix);
	fprintf(fid, "            field_file = %i,\n", (int)mo->fieldfile);
	fprintf(fid, "            field = %i,\n", mo->fieldnum);
	if (mo->parity)
		fprintf(fid, "            parity = True,\n");
	fprintf(fid, "            stars_in_field=%i,\n", (int)mo->ninfield);
	fprintf(fid, "            stars_overlap=%i,\n", (int)mo->noverlap);
	fprintf(fid, "            overlap=%f,\n", mo->overlap);
	fprintf(fid, "            quads_tried=%i,\n", mo->quads_tried);
	fprintf(fid, "            codes_matched=%i,\n", mo->quads_matched);
	fprintf(fid, "            quads_with_ok_scale=%i,\n", mo->quads_scaleok);
	fprintf(fid, "            matches_verified=%i,\n", mo->nverified);
	fprintf(fid, "            objs_used=%i,\n", mo->objs_tried);
	fprintf(fid, "            time_used=%f,\n", mo->timeused);

	fprintf(fid, "            quad=%u,\n", mo->quadno);
	fprintf(fid, "            starids_ABCD=(%u,%u,%u,%u),\n",
			mo->star[0], mo->star[1], mo->star[2], mo->star[3]);
	fprintf(fid, "            field_objects_ABCD=(%u,%u,%u,%u),\n",
			mo->field[0], mo->field[1], mo->field[2], mo->field[3]);
	fprintf(fid, "            min_xyz=(%lf,%lf,%lf), min_radec=(%lf,%lf),\n",
			mo->sMin[0], mo->sMin[1], mo->sMin[2],
			rad2deg(xy2ra(mo->sMin[0], mo->sMin[1])),
			rad2deg(z2dec(mo->sMin[2])));
	fprintf(fid, "            max_xyz=(%lf,%lf,%lf), max_radec=(%lf,%lf),\n",
			mo->sMax[0], mo->sMax[1], mo->sMax[2],
			rad2deg(xy2ra(mo->sMax[0], mo->sMax[1])),
			rad2deg(z2dec(mo->sMax[2])));
	
	if (mo->transform_valid) {
		fprintf(fid, "            transform=[%.12g,%.12g,%.12g,%.12g,%.12g,%.12g,%.12g,%.12g,%.12g],\n",
				mo->transform[0], mo->transform[1], mo->transform[2],
				mo->transform[3], mo->transform[4], mo->transform[5],
				mo->transform[6], mo->transform[7], mo->transform[8]);
	}
	fprintf(fid, "            code_err = %lf,\n", sqrt(mo->code_err));
	fprintf(fid, "        ),\n");
}

void hits_write_correspondences(FILE* fid, uint* starids, uint* fieldids,
								int Nids, int ok) {
	int i;
	fprintf(fid, "    # Field Object <--> Catalogue Object Mapping Table\n");
	if (!ok) {
		fprintf(fid, "    # warning: some matches agree on resolve but not on mapping\n");
		fprintf(fid, "    resolve_mapping_mismatch=True,\n");
	}
    
	fprintf(fid, "    field2catalog={\n");
	for (i= 0 ; i<Nids; i++)
		fprintf(fid, "        %u : %u,\n", fieldids[i], starids[i]);
	fprintf(fid, "    },\n");
	fprintf(fid, "    catalog2field={\n");
	for (i=0; i<Nids; i++)
		fprintf(fid, "        %u : %u,\n", starids[i], fieldids[i]);
	fprintf(fid, "    },\n");
}

