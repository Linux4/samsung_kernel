#ifndef __UAPI_CAM_SBI_H__
#define __UAPI_CAM_SBI_H__

#include <camera/media/cam_defs.h>
#include <camera/media/cam_cpas.h>

/* Custom driver name */
#define CAM_SBI_DEV_NAME                    "cam-sbi"

/* start of todo : delete */
enum CAM_SBI_IO_TYPE {
	CAM_SBI_IO_TYPE_TAR,
	CAM_SBI_IO_TYPE_REF,
	CAM_SBI_IO_TYPE_RES,
	CAM_SBI_IO_TYPE_DS2,
};

enum cam_custom_hw_task_type {
	CAM_CUSTOM_EVENT_INVALID,
	CAM_CUSTOM_EVENT_TASK1,
	CAM_CUSTOM_EVENT_TASK2,
	CAM_CUSTOM_EVENT_TASK3,
	CAM_CUSTOM_EVENT_MAX,
};

#define CAM_CUSTOM_OUT_RES_UDI_0               1
#define CAM_CUSTOM_OUT_RES_UDI_1               2
#define CAM_CUSTOM_OUT_RES_UDI_2               3

/* To be used for SBI RDI DUMP */
#define CAM_CUSTOM_OUT_RES_UDI_3               4
#define CAM_CUSTOM_OUT_RES_DUMMY               0xDADADADA

#define CAM_SBI_INPUT_PORT_TYPE_TAR (1 << 0)
#define CAM_SBI_INPUT_PORT_TYPE_REF (1 << 1)

#define CAM_SBI_OUTPUT_PORT_TYPE_DS2 (1 << 0)
#define CAM_SBI_OUTPUT_PORT_TYPE_RES (1 << 1)
/* end of todo : delete */

#define CAM_SBI_NUM_SUB_DEVICES             2

/* HW type */
#define CAM_SBI_HW1                         0
#define CAM_SBI_HW2                         1

/* Resource ID */
#define CAM_SBI_RES_ID_PORT                 0

/* Packet opcode for Custom */
#define CAM_SBI_PACKET_OP_BASE              0
#define CAM_SBI_PACKET_INIT_DEV             1
#define CAM_SBI_PACKET_UPDATE_DEV           2
#define CAM_SBI_PACKET_RESTART_DEV          3
#define CAM_SBI_PACKET_OP_MAX               4

#define CAM_CUSTOM_VC_DT_CFG    4
#define CAM_CUSTOM_IN_RES_BASE                0x5000
#define CAM_CUSTOM_IN_RES_PHY_0               (CAM_CUSTOM_IN_RES_BASE + 1)
#define CAM_CUSTOM_IN_RES_PHY_1               (CAM_CUSTOM_IN_RES_BASE + 2)
#define CAM_CUSTOM_IN_RES_PHY_2               (CAM_CUSTOM_IN_RES_BASE + 3)
#define CAM_CUSTOM_IN_RES_PHY_3               (CAM_CUSTOM_IN_RES_BASE + 4)


/* Query devices */
/**
 * struct cam_sbi_dev_cap_info - A cap info for particular hw type
 *
 * @hw_type:            Custom HW type
 * @hw_version:         Hardware version
 *
 */
struct cam_sbi_dev_cap_info {
	__u32              hw_type;
	__u32              hw_version;
};

/**
 * struct cam_sbi_query_cap_cmd - Custom HW query device capability payload
 *
 * @device_iommu:               returned iommu handles for device
 * @num_dev:                    returned number of device capabilities
 * @reserved:                   reserved field for alignment
 * @dev_caps:                   returned device capability array
 *
 */
struct cam_sbi_query_cap_cmd {
	struct cam_iommu_handle         device_iommu;
	__s32                         num_dev;
	__u32                        reserved;
	struct cam_iommu_handle cdm_iommu;
	__u32 num_devices; 	/* todo : delete num_devices */
	struct cam_sbi_dev_cap_info  dev_caps[CAM_SBI_NUM_SUB_DEVICES];
};

/* Acquire Device */
/**
 * struct cam_sbi_out_port_info - An output port resource info
 *
 * @res_type:              output resource type
 * @sbi_info1-4         sbi params
 * @reserved               reserved field for alignment
 *
 */
struct cam_sbi_out_port_info {
	__u32                res_type;
	__u32                format;
	__u32                custom_info1;
	__u32                custom_info2;
	__u32                custom_info3;
	__u32                reserved;
};

/**
 * struct cam_sbi_in_port_info - An input port resource info
 *
 * @res_type:              input resource type
 * @sbi_info1-4         sbi params
 * @num_out_res:           number of the output resource associated
 * @data:                  payload that contains the output resources
 *
 */
struct cam_sbi_in_port_info {
	__u32                        res_type;
	__u32                        lane_type;
	__u32                        lane_num;
	__u32                        lane_cfg;
	__u32                        vc[CAM_CUSTOM_VC_DT_CFG];
	__u32                        dt[CAM_CUSTOM_VC_DT_CFG];
	__u32                        num_valid_vc_dt;
	__u32                        format;
	__u32                        test_pattern;
	__u32                        usage_type;
	__u32                        left_start;
	__u32                        left_stop;
	__u32                        left_width;
	__u32                        right_start;
	__u32                        right_stop;
	__u32                        right_width;
	__u32                        line_start;
	__u32                        line_stop;
	__u32                        height;
	__u32                        pixel_clk;
	__u32                        num_bytes_out;
	__u32                        custom_info1;
	__u32                        custom_info2;
	__u32                        custom_info3;
	__u32                        num_out_res;
	struct cam_sbi_out_port_info data[1];
};

/**
 * struct cam_sbi_resource - A resource bundle
 *
 * @resoruce_id:                resource id for the resource bundle
 * @length:                     length of the while resource blob
 * @handle_type:                type of the resource handle
 * @reserved:                   reserved field for alignment
 * @res_hdl:                    resource handle that points to the
 *                              resource array;
 */
struct cam_sbi_resource {
	__u32                       resource_id;
	__u32                       length;
	__u32                       handle_type;
	__u32                       reserved;
	__u64                       res_hdl;
};

/**
 * struct cam_custom_acquire_hw_info - Custom acquire HW params
 *
 * @num_inputs           : Number of inputs
 * @input_info_size      : Size of input info struct used
 * @input_info_offset    : Offset of input info from start of data
 * @reserved             : reserved
 * @data                 : Start of data region
 */
struct cam_custom_acquire_hw_info {
	__u32                num_inputs;
	__u32                input_info_size;
	__u32                input_info_offset;
	__u32                reserved;
	__u64                data;
};

/**
 * struct cam_sbi_cmd_buf_type_init - cmd buf type init when it use CAM_SBI_PACKET_INIT_DEV time
 *
 * @custom_info:        sbi info
 * @scratch_buf_hdl:    mem handle for scratch buffer
 * @register_set       initialize register set [addr][val] : temp
 */
struct cam_sbi_cmd_buf_type_init {
	__u32 custom_info;
	__s32 scratch_buf_hdl;
	__s32 preview_buffer_len;
	__s64 record_buffer_len;
	__s32 sensor_width;
	__s32 sensor_height;
	__s32 frame_size;
	__s32 batch_num;
	__s32 max_frames;
	__u32 is_ssm;
	__u32 umd_node_type;    /* 0 : normal, 10 : SSM recording */
	__u32 cue_option;       /* 0 : maunual, 1 : auto */
	__u32 ssm_framerate;    /* 960 = 960 fps, 480 = 480 fps */
	__u64 clock_rate;

	/* TODO: should be modify register set buffer size */
	__u32    register_set_size;
	__u32    register_set[500][2];    /* [register address offset][value] */
};

/**
 * struct cam_sbi_bw_config_v2 - Bandwidth configuration
 *
 * @usage_type:                 Usage type (Single/Dual)
 * @num_paths:                  Number of axi data paths
 * @axi_path                    Per path vote info
 */
struct cam_sbi_bw_config_v2 {
	/* __u32                             usage_type; */
	__u32                             num_paths;
	struct cam_axi_per_path_bw_vote      axi_path[30]; /* CAM_SBI_MAX_PER_PATH_VOTES */
} __attribute__((packed));

/**
 * struct cam_sbi_cmd_buf_type_1 - cmd buf type 1
 *
 * @custom_info:                sbi info
 * @reserved:                   reserved
 * @scratch_buf_hdl:            mem handle for scratch buffer
 */
struct cam_sbi_cmd_buf_type_1 {
	__u32 custom_info;
	__u32 umd_node_type;    /* 0 : normal, 10 : SSM recording */
	__s32 scratch_buf_hdl;
	__u32 task2_setup;
	struct cam_sbi_bw_config_v2 bw_config_v2;
	__u32 is_ssm;
	__u32 cue_option;       /* 0 : maunual, 1 : auto */
	__u32 ssm_framerate;    /* 960 = 960 fps, 480 = 480 fps */
	__u32 ssm_maxframes;
	__u32 task2_action;

	__u32 register_set_size;
	__u32 register_set[20][2]; /* [register address offset][value] */
};

/**
 * struct cam_sbi_cmd_buf_type_2 - cmd buf type 2
 *
 * @sbi_info1:               Custom info 1
 * @sbi_info2:               Custom info 2
 * @sbi_info3:               Custom info 3
 * @reserved:                   reserved
 */
struct cam_sbi_cmd_buf_type_2 {
	__u32                       custom_info1;
	__u32                       custom_info2;
	__u32                       custom_info3;
	__u32                       reserved;
};


/* todo : delete below */
struct cam_sbi_soc_info {
	__u64 clock_rate;
	__u64 bandwidth;
	__u64 reserved[4];
};

struct cam_sbi_acquire_args {
	struct cam_sbi_soc_info sbi_soc_info;
};

#endif /* __UAPI_CAM_SBI_H__ */

