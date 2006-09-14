#define LOW_HR_D(kd, D, i) ((kd)->bb.d + (2*(i)*(D)))
#define HIGH_HR_D(kd, D, i) ((kd)->bb.d + ((2*(i)+1)*(D)))

#define LOW_HR_F(kd, D, i) ((kd)->bb.f + (2*(i)*(D)))
#define HIGH_HR_F(kd, D, i) ((kd)->bb.f + ((2*(i)+1)*(D)))

#define LOW_HR_I(kd, D, i) ((kd)->bb.i + (2*(i)*(D)))
#define HIGH_HR_I(kd, D, i) ((kd)->bb.i + ((2*(i)+1)*(D)))

#define LOW_HR_S(kd, D, i) ((kd)->bb.s + (2*(i)*(D)))
#define HIGH_HR_S(kd, D, i) ((kd)->bb.s + ((2*(i)+1)*(D)))

#define KD_SPLIT_I(kd, i) ((kd)->split.i + (i))
#define KD_SPLIT_S(kd, i) ((kd)->split.s + (i))
#define KD_SPLIT_F(kd, i) ((kd)->split.f + (i))
#define KD_SPLIT_D(kd, i) ((kd)->split.d + (i))

#define DOUBLE2U32(kd, d, r)   ((u32)rint(((r) - (kd)->minval[d]) * (kd)->scale[d]))

#define DOUBLE2U16(kd, d, r)   ((u16)rint(((r) - (kd)->minval[d]) * (kd)->scale[d]))

#define U32TODOUBLE(kd, d, t)  ( (((double)(t)) / ((kd)->scale[d])) + (kd)->minval[d] )

#define U16TODOUBLE(kd, d, t)  ( (((double)(t)) / ((kd)->scale[d])) + (kd)->minval[d] )

