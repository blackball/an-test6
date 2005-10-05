#include <math.h>
#include <stdio.h>
#include "dualtree.h"
#include "KD/ambs.h"
#include "KD/distances.h"
#include "KD/hrect.h"
#include "KD/amiv.h"

bool within_range(void* params, node* search, node* query);
void print_result(void* params, node* search, node* query);

struct params {
    // radius-squared of the search range.
    double maxdistsq;
};
typedef struct params params;

// some accounting stats
// number of distance computations
int distance_computations = 0;
// number of pairs of leaf nodes returned
int leaf_pairs = 0;
// number of pairs of points rejected by box-to-box pruning.
int rejected_in_boxes = 0;
// number of pairs of points rejected by point-to-box pruning.
int rejected_point_box = 0;
// number of search boxes searched.
int searched_boxes = 0;
// number of search boxes accepted without checking individual points.
int accepted_boxes = 0;
// number of points accepted because their box was accepted.
int accepted_in_boxes = 0;
// number of points rejected individually.
int rejected_points = 0;
// number of points accepted individually.
int accepted_points = 0;

FILE* graphf = NULL;
int graphid = 0;
double graphscale = 8.0;

void draw_node(FILE* f, node* n, char* color) {
    //hrect* rect = mk_scale_hrect(n->box, graphscale);
    int id = graphid++;
    hrect* rect = n->box;
    // draw the box slightly smaller than its real size so that bordering
    // edges don't overlap too much.
    //double offset = 0.002;
    double offset = 0.0;
    fprintf(f, "  NA%i [ pos=\"%f,%f!\" ];\n",
            id,
            graphscale * (hrect_lo_ref(rect, 0) + offset),
            graphscale * (hrect_lo_ref(rect, 1) + offset));
    fprintf(f, "  NB%i [ pos=\"%f,%f!\" ];\n",
            id,
            graphscale * (hrect_lo_ref(rect, 0) + offset),
            graphscale * (hrect_hi_ref(rect, 1) - offset));
    fprintf(f, "  NC%i [ pos=\"%f,%f!\" ];\n",
            id,
            graphscale * (hrect_hi_ref(rect, 0) - offset),
            graphscale * (hrect_hi_ref(rect, 1) - offset));
    fprintf(f, "  ND%i [ pos=\"%f,%f!\" ];\n",
            id,
            graphscale * (hrect_hi_ref(rect, 0) - offset),
            graphscale * (hrect_lo_ref(rect, 1) + offset));
    // if the box was rejected, draw its outline in blue.
    // accepted, gray.
    fprintf(f, "  NA%i -- NB%i -- NC%i -- ND%i -- NA%i [ "
            "color=%s ];\n", id, id, id, id, id, color);
    //free_hrect(rect);
}

void draw_point(FILE* f, dyv* pt, double diam, char* color) {
    int id = graphid++;
    fprintf(f, "  N%i [ pos=\"%f,%f!\", shape=point, width=%f, "
            "color=\"%s\" ];\n",
            id,
            graphscale * dyv_ref(pt, 0),
            graphscale * dyv_ref(pt, 1),
            graphscale * diam,
            color);
}

void draw_circle(FILE* f, dyv* center, double diam, char* color) {
    int id = graphid++;
    fprintf(f, "  N%i [ pos=\"%f,%f!\", shape=circle, width=%f, "
            "color=\"%s\" ];\n",
            id,
            graphscale * dyv_ref(center, 0),
            graphscale * dyv_ref(center, 1),
            graphscale * diam,
            color);
}

bool contains_pindex(node* n, int index) {
    if (node_is_leaf(n)) {
        return is_in_ivec(n->pindexes, index);
    }
    return (contains_pindex(n->child1, index) ||
            contains_pindex(n->child2, index));
}

// dimension of the space
int D = 2;

// draw a picture for query point #graphpoint.
int graphpoint = 0;

int main() {
    // generate a set of search points and a set of query points.
    // create kd-trees.
    // search for points in the search tree that are within a radius of
    // each point in the query tree.

    // search and query kd-trees
    kdtree* stree = NULL;
    kdtree* qtree = NULL;

    // maximum number of points in a leaf node.
    int Nleaf = 5;

    // number of search points
    int Nsearch = 1000;

    // number of query points
    int Nquery = 500;

    // radius of the search
    double radius = 0.05;

    // arrays for search and query points
    dyv_array* sarray = NULL;
    dyv_array* qarray = NULL;

    int i, d;

    params range_params;

    graphpoint = int_random(Nquery);
    graphf = stderr;
    if (graphf) {
        fprintf(graphf, "graph G {\n");
        fprintf(graphf, "  graph [ sep=0 ];\n");
        fprintf(graphf, "  node [ shape=point, width=0, "
                "fixedsize=true, label=\"\" ];\n");
        fprintf(graphf, "  edge [ headclip=false, tailclip=false ];\n");
    }

    // seed random number generator.
    am_randomize();

    // create random search points.
    sarray = mk_dyv_array(Nsearch);
    for (i=0; i<Nsearch; i++) {
        dyv* v = mk_dyv(D);
        for (d=0; d<D; d++) {
            dyv_ref(v, d) = range_random(0.0, 1.0);
        }
        dyv_array_ref(sarray, i) = v;
    }

    // create search tree
    stree = mk_kdtree_from_points(sarray, Nleaf);

    // create random query points.
    qarray = mk_dyv_array(Nquery);
    for (i=0; i<Nquery; i++) {
        dyv* v = mk_dyv(D);
        for (d=0; d<D; d++) {
            dyv_ref(v, d) = range_random(0.0, 1.0);
        }
        dyv_array_ref(qarray, i) = v;
    }

    // create search tree
    qtree = mk_kdtree_from_points(qarray, Nleaf);

    // set search params
    range_params.maxdistsq = radius*radius;

    // run dual-tree search
    dual_tree_search(stree, qtree,
                     within_range, &range_params,
                     print_result, &range_params);


    // print stats
    printf("\n\nStats:\n");
    printf("distance computations: %i\n", distance_computations);
    printf("leaf pairs: %i\n", leaf_pairs);
    printf("rejected in boxes: %i\n", rejected_in_boxes);
    printf("rejected by point-box: %i\n", rejected_point_box);
    printf("searched boxes: %i\n", searched_boxes);
    printf("accepted boxes: %i\n", accepted_boxes);
    printf("accepted in boxes: %i\n", accepted_in_boxes);
    printf("rejected points: %i\n", rejected_points);
    printf("accepted points: %i\n", accepted_points);

    if (graphf) {
        fprintf(graphf, "}\n");
    }

    return 0;
}



bool within_range(void* vparams, node* search, node* query) {
    params* p = (params*)vparams;
    double mindistsq;
    double maxdistsq;
    bool result;

    // compute the minimum distance between the search and query nodes.
    // if it is less than the search radius, accept the pair of nodes.

    mindistsq = hrect_hrect_min_dsqd(search->box, query->box);
    maxdistsq = hrect_hrect_max_dsqd(search->box, query->box);

    distance_computations++;

    result = (mindistsq <= p->maxdistsq);

    printf("decision: search node rectangle:\n");
    phrect(search->box);

    printf("decision: search node volume: %8g\n", hrect_area(search->box));

    printf("decision: query node rectangle:\n");
    phrect(query->box);

    printf("decision: query node volume: %8g\n", hrect_area(query->box));

    printf("decision: range [%8g, %8g], limit %8g => %s\n",
           sqrt(mindistsq), sqrt(maxdistsq), sqrt(p->maxdistsq),
           result?"accept":"reject");

    // draw picture.
    if ((D>=2) && graphf && contains_pindex(query, graphpoint)) {
        if (!result) {
            draw_node(graphf, search, "gray");
        }
    }

    if (!result) {
        rejected_in_boxes += (search->num_points * query->num_points);
    }

    return result;
}

void print_result(void* vparams, node* search, node* query) {
    params* p = (params*)vparams;

    printf("results: search node rectangle:\n");
    phrect(search->box);

    printf("results: query node rectangle:\n");
    phrect(query->box);

    // this function gets called on the children of accepted nodes, so this
    // pair of nodes may not be acceptable.
    if (!within_range(vparams, search, query)) {
        printf("results: not within range.\n");
        return;
    }

    // either node can be a non-leaf.  in this case, recurse until we
    // hit leaves.
    if (!node_is_leaf(search)) {
        printf("results: recursing on search node.\n");
        print_result(vparams, search->child1, query);
        print_result(vparams, search->child2, query);
        return;
    }

    if (!node_is_leaf(query)) {
        printf("results: recursing on query node.\n");
        print_result(vparams, search, query->child1);
        print_result(vparams, search, query->child2);
        return;
    }

    // okay, here we are, we've got a pair of valid leaf nodes.
    {
        double mindistsq;
        double maxdistsq;
        int i, j;

        mindistsq = hrect_hrect_min_dsqd(search->box, query->box);
        maxdistsq = hrect_hrect_max_dsqd(search->box, query->box);

        distance_computations++;
        leaf_pairs++;

        printf("results: got leaves: range [%8g, %8g]\n",
               sqrt(mindistsq), sqrt(maxdistsq));

        // look at individual points in the query tree.
        for (i=0; i<query->num_points; i++) {
            dyv* qp = dyv_array_ref(query->points, i);
            // if the search node contains a lot of points, this check
            // (from a query point to a search box)
            // can avoid a lot of work.  If there aren't very many points,
            // it 's more effort than it's worth most of the time.
            mindistsq = hrect_dyv_min_dsqd(search->box, qp);

            distance_computations++;

            if ((D>=2) && graphf &&
                (ivec_ref(query->pindexes, i)==graphpoint)) {
                /*
                  draw_point(graphf, qp, 0.025, "black");
                */
                draw_point(graphf, qp, 0.01, "black");
                draw_circle(graphf, qp, 2.0*sqrt(p->maxdistsq), "black");
                draw_node(graphf, query, "black");
            }

            if (mindistsq > p->maxdistsq) {
                printf("results: query point %i: rejecting search box: dist "
                       "%8g\n", i, sqrt(mindistsq));

                rejected_point_box += (search->num_points);

                if ((D>=2) && graphf &&
                    (ivec_ref(query->pindexes, i)==graphpoint)) {
                    draw_node(graphf, search, "maroon");
                }

                continue;
            }
            // search box is okay.

            searched_boxes++;

            // if the search radius is large, or the search boxes contain
            // a lot of points, this check can be worthwhile - is the whole
            // box going to be accepted?
            maxdistsq = hrect_dyv_max_dsqd(search->box, qp);

            distance_computations++;

            if (maxdistsq <= p->maxdistsq) {
                // accept every point in the box!

                accepted_boxes++;
                accepted_in_boxes += search->num_points;

                printf("results: accepting whole box: maxdist %8g, npoints "
                       "%i.\n", sqrt(maxdistsq), search->num_points);
                for (j=0; j<search->num_points; j++) {
                    dyv* sp = dyv_array_ref(search->points, j);
                    double distsq = dyv_dyv_dsqd(qp, sp);
                    printf("results: query point %i: accept search point "
                           "%i: dist %8g\n", i, j, sqrt(distsq));
                }

                if ((D>=2) && graphf &&
                    (ivec_ref(query->pindexes, i)==graphpoint)) {
                    draw_node(graphf, search, "blue");
                }

                continue;
            }

            if ((D>=2) && graphf &&
                (ivec_ref(query->pindexes, i)==graphpoint)) {
                draw_node(graphf, search, "red");
            }

            for (j=0; j<search->num_points; j++) {
                dyv* sp = dyv_array_ref(search->points, j);
                double distsq = dyv_dyv_dsqd(qp, sp);
                bool accept = (distsq <= p->maxdistsq);
                if ((D>=2) && graphf &&
                    (ivec_ref(query->pindexes, i)==graphpoint)) {
                    draw_point(graphf, sp, 0.01, (accept?"blue":"magenta"));
                }
                if (!accept) {
                    printf("results: query point %i: reject search point %i:"
                           " dist %8g\n", i, j, sqrt(distsq));

                    rejected_points++;

                    continue;
                }
                // we've found a good pair!
                printf("results: query point %i: accept search point %i: "
                       "dist %8g\n", i, j, sqrt(distsq));

                accepted_points++;
            }
        }
    }
}

