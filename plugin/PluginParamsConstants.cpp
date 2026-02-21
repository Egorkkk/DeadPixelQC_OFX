#include "PluginParams.h"

namespace DeadPixelQC_OFX {

// Parameter names
const char* PARAM_ENABLE = "Enable";
const char* PARAM_VIEW_MODE = "ViewMode";
const char* PARAM_LUMA_THRESHOLD = "LumaThreshold";
const char* PARAM_WHITENESS_THRESHOLD = "WhitenessThreshold";
const char* PARAM_NEIGHBORHOOD = "Neighborhood";
const char* PARAM_ROBUST_Z = "RobustZ";
const char* PARAM_MAX_CLUSTER_AREA = "MaxClusterArea";
const char* PARAM_TEMPORAL_MODE = "TemporalMode";
const char* PARAM_STUCK_WINDOW_FRAMES = "StuckWindowFrames";
const char* PARAM_STUCK_MIN_FRACTION = "StuckMinFraction";
const char* PARAM_MAX_GAP_FRAMES = "MaxGapFrames";
const char* PARAM_REPAIR_ENABLE = "RepairEnable";
const char* PARAM_REPAIR_METHOD = "RepairMethod";

// Workflow mode and status parameters
const char* PARAM_WORKFLOW_MODE = "WorkflowMode";
const char* PARAM_EVENTS_COUNT = "EventsCount";
const char* PARAM_SELECTED_EVENT_INDEX = "SelectedEventIndex";
const char* PARAM_SELECTED_EVENT_INFO = "SelectedEventInfo";
const char* PARAM_MAP_SIZE = "MapSize";
const char* PARAM_MAP_STATUS = "MapStatus";

// Workflow mode button parameters
const char* PARAM_PREV_EVENT = "PrevEvent";
const char* PARAM_NEXT_EVENT = "NextEvent";
const char* PARAM_CLEAR_EVENTS = "ClearEvents";
const char* PARAM_EXPORT_EVENTS = "ExportEvents";
const char* PARAM_LOAD_MAP = "LoadMap";
const char* PARAM_SAVE_MAP = "SaveMap";
const char* PARAM_RESET_MAP = "ResetMap";
const char* PARAM_FINALIZE_MAP = "FinalizeMap";

// View mode enum strings
const char* VIEW_MODE_OUTPUT = "Output";
const char* VIEW_MODE_CANDIDATES_OVERLAY = "CandidatesOverlay";
const char* VIEW_MODE_CONFIRMED_OVERLAY = "ConfirmedOverlay";
const char* VIEW_MODE_MASK_ONLY = "MaskOnly";

// Neighborhood enum strings
const char* NEIGHBORHOOD_3X3 = "3x3";
const char* NEIGHBORHOOD_5X5 = "5x5";

// Temporal mode enum strings
const char* TEMPORAL_MODE_OFF = "Off";
const char* TEMPORAL_MODE_SEQUENTIAL_ONLY = "SequentialOnly";

// Repair method enum strings
const char* REPAIR_METHOD_NEIGHBOR_MEDIAN = "NeighborMedian";
const char* REPAIR_METHOD_DIRECTIONAL_MEDIAN = "DirectionalMedian";

// Workflow mode enum strings
const char* WORKFLOW_MODE_QC_SCAN = "QC_Scan";
const char* WORKFLOW_MODE_MAP_BUILD = "Map_Build";
const char* WORKFLOW_MODE_MAP_APPLY = "Map_Apply";

// Default values
const double DEFAULT_LUMA_THRESHOLD = 0.98;
const double DEFAULT_WHITENESS_THRESHOLD = 0.05;
const double DEFAULT_ROBUST_Z_3X3 = 10.0;
const double DEFAULT_ROBUST_Z_5X5 = 8.0;
const int DEFAULT_MAX_CLUSTER_AREA = 4;
const int DEFAULT_STUCK_WINDOW_FRAMES = 60;
const double DEFAULT_STUCK_MIN_FRACTION = 0.95;
const int DEFAULT_MAX_GAP_FRAMES = 2;

// Default workflow values
const int DEFAULT_WORKFLOW_MODE = 0; // QC_Scan
const char* DEFAULT_EVENTS_COUNT = "0";
const int DEFAULT_SELECTED_EVENT_INDEX = -1;
const char* DEFAULT_SELECTED_EVENT_INFO = "None";
const char* DEFAULT_MAP_SIZE = "0";
const char* DEFAULT_MAP_STATUS = "Empty";

} // namespace DeadPixelQC_OFX