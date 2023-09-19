typedef unsigned int uint_;

__kernel void sum1(__global const uint_ *src, __global uint_ *res, const uint_ size) {
    const uint_ id = get_global_id(0);

    if (id >= size) {
        return;
    }

    atomic_add(res, src[id]);
}

#define VALUES_PER_WORK_ITEM 128

__kernel void sum2(__global const uint_ *src, __global uint_ *res, const uint_ size) {
    const uint_ id = get_global_id(0);
    uint_ value = 0;
    for (uint_ i = 0; i < VALUES_PER_WORK_ITEM; i++) {
        uint_ indx = id * VALUES_PER_WORK_ITEM + i;

        if (indx < size) {
            value += src[indx];
        }
    }
    atomic_add(res, value);
}

__kernel void sum3(__global const uint_ *src, __global uint_ *res, const uint_ size) {
    const uint_ gid = get_group_id(0);
    const uint_ lid = get_local_id(0);
    const uint_ wsize = get_local_size(0);
    uint_ value = 0;
    for (uint_ i = 0; i < VALUES_PER_WORK_ITEM; i++) {
        uint_ indx = gid * wsize * VALUES_PER_WORK_ITEM + wsize * i + lid;

        if (indx < size) {
            value += src[indx];
        }
    }
    atomic_add(res, value);
}

#define WORKGROUP_SIZE 128

__kernel void sum4(__global const uint_ *src, __global uint_ *res, const uint_ size) {
    const uint_ gid = get_global_id(0);
    const uint_ lid = get_local_id(0);

    __local uint_ buff[WORKGROUP_SIZE];
    buff[lid] = src[gid];

    barrier(CLK_LOCAL_MEM_FENCE);

    if (lid == 0) {
        uint_ value = 0;
        for (uint_ i = 0; i < WORKGROUP_SIZE; i++) {
            value += buff[i];
        }
        atomic_add(res, value);
    }
}