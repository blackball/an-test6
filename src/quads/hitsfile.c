#include "hitsfile.h"

void hits_header_init(hits_header* h) {
	h->field_file_name = NULL;
	h->tree_file_name = NULL;
	h->nfields = 0;
	h->ncodes = 0;
	h->nstars = 0;
	h->codetol = 0.0;
	h->agreetol = 0.0;
	h->parity = FALSE;
	h->min_matches_to_agree = 0;
	h->max_matches_needed = 0;
}

void hits_write_header(FILE* fid, hits_header* h) {
    fprintf(fid, "# SOLVER PARAMS:\n");
    fprintf(fid, "# solving fields in %s using %s\n",
			h->field_file_name, h->tree_file_name);
    fprintf(fid, "field_to_solve = '%s'\n", h->field_file_name);
    fprintf(fid, "index_used = '%s'\n", h->tree_file_name);
	fprintf(fid, "nfields = %u\n", h->nfields);
	fprintf(fid, "ncodes = %u\n", h->ncodes);
	fprintf(fid, "nstars = %u\n", h->nstars);
    fprintf(fid, "code_tol = %g\n", h->codetol);
    fprintf(fid, "agree_tol = %g\n", h->agreetol);
	fprintf(fid, "parity_flip = %s\n", (h->parity ? "True" : "False"));
	if (h->parity)
		fprintf(fid, "# flipping parity (swapping row/col image coordinates)\n");
    fprintf(fid, "min_matches_to_agree = %u\n", h->min_matches_to_agree);
    fprintf(fid, "max_matches_needed = %u\n", h->max_matches_needed);

    fprintf(fid, "############################################################\n");
    fprintf(fid, "# Result data, stored as a list of dictionaries\n");
    fprintf(fid, "results = [ \n");
}

void hits_write_tailer(FILE* fid) {
    fprintf(fid, "] \n");
    fprintf(fid, "# END OF RESULTS\n");
    fprintf(fid, "############################################################\n");
}

void hits_field_init(hits_field* h) {
	h->user_quit = FALSE;
	h->field = 0;
	h->objects_in_field = 0;
	h->objects_examined = 0;
	h->field_corners = NULL;
	h->ntries = 0;
	h->nmatches = 0;
	h->nagree = 0;
	h->parity = FALSE;
}

void hits_write_field_header(FILE* fid, hits_field* h) {
	fprintf(fid, "# --------------------\n");
	fprintf(fid, "dict(\n");
	fprintf(fid, "    user_quit=%s,\n", (h->user_quit?"True":"False"));
	fprintf(fid, "    field=%u,\n", h->field);
	fprintf(fid, "    objects_in_field=%u,\n", h->objects_in_field);
	fprintf(fid, "    objects_examined=%u,\n", h->objects_examined);
	fprintf(fid, "    # Image corners\n");
	if (h->field_corners) {
		xy* c = h->field_corners;
		if (h->parity) {
			fprintf(fid, "    min_uv_corner=(%g,%g), max_uv_corner=(%g,%g),\n",
					xy_refy(c, 0), xy_refx(c, 0),
					xy_refy(c, 1), xy_refx(c, 1));
		} else {
			fprintf(fid, "    min_uv_corner=(%g,%g), max_uv_corner=(%g,%g),\n",
					xy_refx(c, 0), xy_refy(c, 0),
					xy_refx(c, 1), xy_refy(c, 1));
		}
	}
	fprintf(fid, "    quads_tried=%i,\n", h->ntries);
	fprintf(fid, "    codes_matched=%i,\n", h->nmatches);
	fprintf(fid, "    # %d matches agree on resolving of the field:\n", h->nagree);
	fprintf(fid, "    matches_agree=%d,\n", h->nagree);
	fprintf(fid, "    quads=[\n");
}


void hits_write_field_tailer(FILE* fid) {
	fprintf(fid, "    ],\n");
	fprintf(fid, "),\n");
}

void hits_write_hit(FILE* fid, MatchObj* mo) {
	if (!mo) {
		fprintf(fid, "        # No agreement between matches. Could not resolve field.\n");
		return;
	}
	fprintf(fid, "        dict(\n");
	fprintf(fid, "            quad=%lu,\n", mo->quadno);
	fprintf(fid, "            starids_ABCD=(%lu,%lu,%lu,%lu),\n",
			mo->iA, mo->iB, mo->iC, mo->iD);
	fprintf(fid, "            field_objects_ABCD=(%lu,%lu,%lu,%lu),\n",
			mo->fA, mo->fB, mo->fC, mo->fD);
	fprintf(fid, "            min_xyz=(%lf,%lf,%lf), min_radec=(%lf,%lf),\n",
			star_ref(mo->sMin, 0), star_ref(mo->sMin, 1), star_ref(mo->sMin, 2),
			rad2deg(xy2ra(star_ref(mo->sMin, 0), star_ref(mo->sMin, 1))),
			rad2deg(z2dec(star_ref(mo->sMin, 2))));
	fprintf(fid, "            max_xyz=(%lf,%lf,%lf), max_radec=(%lf,%lf),\n",
			star_ref(mo->sMax, 0), star_ref(mo->sMax, 1), star_ref(mo->sMax, 2),
			rad2deg(xy2ra(star_ref(mo->sMax, 0), star_ref(mo->sMax, 1))),
			rad2deg(z2dec(star_ref(mo->sMax, 2))));
	/*
	  fprintf(fid, "            transform=array([%.12g,%.12g,%.12g,%.12g,%.12g,%.12g,%.12g,%.12g,%.12g]),\n",
	  mo->transform[0], mo->transform[1], mo->transform[2],
	  mo->transform[3], mo->transform[4], mo->transform[5],
	  mo->transform[6], mo->transform[7], mo->transform[8]);
	  fprintf(fid, "            # T=[%.12g,%.12g,%.12g;%.12g,%.12g,%.12g;%.12g,%.12g,%.12g],\n",
	  mo->transform[0], mo->transform[1], mo->transform[2],
	  mo->transform[3], mo->transform[4], mo->transform[5],
	  mo->transform[6], mo->transform[7], mo->transform[8]);
	  fprintf(fid, "            corner1=[%.12g,%.12g,1.0],\n",
	  mo->corners[0], mo->corners[1]);
	  fprintf(fid, "            corner2=[%.12g,%.12g,1.0],\n",
	  mo->corners[2], mo->corners[3]);
	  fprintf(fid, "            starA=[%.12g,%.12g,%.12g],\n",
	  mo->starA[0], mo->starA[1], mo->starA[2]);
	  fprintf(fid, "            fieldA=[%.12g,%.12g,1.0],\n",
	  mo->fieldA[0], mo->fieldA[1]);
	  fprintf(fid, "            starB=[%.12g,%.12g,%.12g],\n",
	  mo->starB[0], mo->starB[1], mo->starB[2]);
	  fprintf(fid, "            fieldB=[%.12g,%.12g,1.0],\n",
	  mo->fieldB[0], mo->fieldB[1]);
	  fprintf(fid, "            starC=[%.12g,%.12g,%.12g],\n",
	  mo->starC[0], mo->starC[1], mo->starC[2]);
	  fprintf(fid, "            fieldC=[%.12g,%.12g,1.0],\n",
	  mo->fieldC[0], mo->fieldC[1]);
	  fprintf(fid, "            starD=[%.12g,%.12g,%.12g],\n",
	  mo->starD[0], mo->starD[1], mo->starD[2]);
	  fprintf(fid, "            fieldD=[%.12g,%.12g,1.0],\n",
	  mo->fieldD[0], mo->fieldD[1]);
	  fprintf(fid, "            abcdorder=%i,\n",
	  mo->abcdorder);
	*/
	if (mo->code_err > 0.0) {
		fprintf(fid, "            code_err=%lf,\n", sqrt(mo->code_err));
	}
	fprintf(fid, "        ),\n");
}

void hits_write_correspondences(FILE* fid, sidx* starids, sidx* fieldids,
								int Nids, int ok) {
	int i;
	fprintf(fid, "    # Field Object <--> Catalogue Object Mapping Table\n");
	if (!ok) {
		fprintf(fid,
				"    # warning -- some matches agree on resolve but not on mapping\n");
	}
	fprintf(fid, "    resolve_mapping_mismatch=%s,\n", (ok?"False":"True"));
    
	fprintf(fid, "    field2catalog={\n");
	for (i= 0 ; i<Nids; i++)
		fprintf(fid, "        %lu : %lu,\n", fieldids[i], starids[i]);
	fprintf(fid, "    },\n");
	fprintf(fid, "    catalog2field={\n");
	for (i=0; i<Nids; i++)
		fprintf(fid, "        %lu : %lu,\n", starids[i], fieldids[i]);
	fprintf(fid, "    },\n");
}

/*
  void hits_write_hit(FILE* fid, hits_hit hit) {
  }
*/
