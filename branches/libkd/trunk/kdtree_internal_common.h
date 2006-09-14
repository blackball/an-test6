#define DOUBLE2U32(      kd, d, r)   ((u32)rint (((r) - (kd)->minval[d]) * (kd)->scale[d]))
#define DOUBLE2U32_FLOOR(kd, d, r)   ((u32)floor(((r) - (kd)->minval[d]) * (kd)->scale[d]))
#define DOUBLE2U32_CEIL( kd, d, r)   ((u32)ceil (((r) - (kd)->minval[d]) * (kd)->scale[d]))

#define DOUBLE2U16(      kd, d, r)   ((u16)rint (((r) - (kd)->minval[d]) * (kd)->scale[d]))
#define DOUBLE2U16_FLOOR(kd, d, r)   ((u16)floor(((r) - (kd)->minval[d]) * (kd)->scale[d]))
#define DOUBLE2U16_CEIL( kd, d, r)   ((u16)ceil (((r) - (kd)->minval[d]) * (kd)->scale[d]))

#define U32TODOUBLE(kd, d, t)  ( (((double)(t)) / ((kd)->scale[d])) + (kd)->minval[d] )

#define U16TODOUBLE(kd, d, t)  ( (((double)(t)) / ((kd)->scale[d])) + (kd)->minval[d] )

