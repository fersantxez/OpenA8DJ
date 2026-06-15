#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub enum Status {
    Pass,
    Fail,
    NotReady,
    BlockedLockBusy,
    BlockedPhysicalCapture,
    BlockedDirtyRoute,
    BlockedUsbEnumeration,
    BlockedIrigUnstable,
    BlockedUnvalidatedDvs,
    BlockedStaleHash,
    SkippedBusy,
}

impl Status {
    pub const fn as_str(self) -> &'static str {
        match self {
            Self::Pass => "PASS",
            Self::Fail => "FAIL",
            Self::NotReady => "NOT_READY",
            Self::BlockedLockBusy => "BLOCKED_LOCK_BUSY",
            Self::BlockedPhysicalCapture => "BLOCKED_PHYSICAL_CAPTURE",
            Self::BlockedDirtyRoute => "BLOCKED_DIRTY_ROUTE",
            Self::BlockedUsbEnumeration => "BLOCKED_USB_ENUMERATION",
            Self::BlockedIrigUnstable => "BLOCKED_IRIG_UNSTABLE",
            Self::BlockedUnvalidatedDvs => "BLOCKED_UNVALIDATED_DVS",
            Self::BlockedStaleHash => "BLOCKED_STALE_HASH",
            Self::SkippedBusy => "SKIPPED_BUSY",
        }
    }
}

#[derive(Clone, Copy, Debug, PartialEq)]
pub struct Limit {
    pub minimum: f64,
    pub stretch: f64,
}

#[derive(Clone, Copy, Debug, PartialEq)]
pub struct RangeLimit {
    pub minimum_low: f64,
    pub minimum_high: f64,
    pub stretch_low: f64,
    pub stretch_high: f64,
}

#[derive(Clone, Copy, Debug, PartialEq)]
pub struct InternalPerformanceThresholds {
    pub start_seconds: Limit,
    pub first_callback_seconds: Limit,
    pub driver_avg_cpu_pct: Limit,
    pub driver_p95_cpu_pct: Limit,
    pub coreaudiod_p95_cpu_pct: Limit,
    pub windowserver_p95_cpu_pct: Limit,
    pub output_read_ratio: Limit,
    pub resets: u64,
    pub underruns: u64,
    pub panics: u64,
    pub mode2_check_errors: u64,
}

#[derive(Clone, Copy, Debug, PartialEq)]
pub struct PhysicalToneThresholds {
    pub capture_peak: RangeLimit,
    pub sideband_ratio: Limit,
    pub segment_sideband_p95: Limit,
    pub segment_sideband_max: Limit,
    pub strongest_sideband_db: Limit,
    pub clicks: u64,
}

#[derive(Clone, Copy, Debug, PartialEq)]
pub struct PhysicalMusicThresholds {
    pub alignment: Limit,
    pub rms: RangeLimit,
    pub cpu_noise_correlation: Limit,
    pub driver_avg_cpu_pct: Limit,
    pub driver_p95_cpu_pct: Limit,
    pub coreaudiod_p95_cpu_pct: Limit,
    pub clicks: u64,
    pub clipped_samples: u64,
}

#[derive(Clone, Copy, Debug, PartialEq)]
pub struct TimecodeThresholds {
    pub drift_ppm_abs: Limit,
    pub edge_jitter_frames_p95: Limit,
    pub polarity_confidence: Limit,
    pub route_flip_glitches: u64,
}

#[derive(Clone, Copy, Debug, PartialEq)]
pub struct PmSuccessThresholds {
    pub internal: InternalPerformanceThresholds,
    pub tone: PhysicalToneThresholds,
    pub music: PhysicalMusicThresholds,
    pub timecode: TimecodeThresholds,
}

impl Default for PmSuccessThresholds {
    fn default() -> Self {
        Self {
            internal: InternalPerformanceThresholds {
                start_seconds: Limit {
                    minimum: 0.25,
                    stretch: 0.10,
                },
                first_callback_seconds: Limit {
                    minimum: 0.30,
                    stretch: 0.12,
                },
                driver_avg_cpu_pct: Limit {
                    minimum: 10.0,
                    stretch: 5.5,
                },
                driver_p95_cpu_pct: Limit {
                    minimum: 12.0,
                    stretch: 6.5,
                },
                coreaudiod_p95_cpu_pct: Limit {
                    minimum: 8.0,
                    stretch: 1.5,
                },
                windowserver_p95_cpu_pct: Limit {
                    minimum: 45.0,
                    stretch: 20.0,
                },
                output_read_ratio: Limit {
                    minimum: 0.90,
                    stretch: 0.995,
                },
                resets: 0,
                underruns: 0,
                panics: 0,
                mode2_check_errors: 0,
            },
            tone: PhysicalToneThresholds {
                capture_peak: RangeLimit {
                    minimum_low: 0.020,
                    minimum_high: 0.920,
                    stretch_low: 0.100,
                    stretch_high: 0.800,
                },
                sideband_ratio: Limit {
                    minimum: 0.008,
                    stretch: 0.004,
                },
                segment_sideband_p95: Limit {
                    minimum: 0.006,
                    stretch: 0.004,
                },
                segment_sideband_max: Limit {
                    minimum: 0.008,
                    stretch: 0.004,
                },
                strongest_sideband_db: Limit {
                    minimum: -43.0,
                    stretch: -50.0,
                },
                clicks: 0,
            },
            music: PhysicalMusicThresholds {
                alignment: Limit {
                    minimum: 0.970,
                    stretch: 0.985,
                },
                rms: RangeLimit {
                    minimum_low: -28.0,
                    minimum_high: -10.0,
                    stretch_low: -24.0,
                    stretch_high: -12.0,
                },
                cpu_noise_correlation: Limit {
                    minimum: 0.08,
                    stretch: 0.04,
                },
                driver_avg_cpu_pct: Limit {
                    minimum: 8.0,
                    stretch: 5.5,
                },
                driver_p95_cpu_pct: Limit {
                    minimum: 12.0,
                    stretch: 6.5,
                },
                coreaudiod_p95_cpu_pct: Limit {
                    minimum: 8.0,
                    stretch: 1.5,
                },
                clicks: 0,
                clipped_samples: 0,
            },
            timecode: TimecodeThresholds {
                drift_ppm_abs: Limit {
                    minimum: 50.0,
                    stretch: 15.0,
                },
                edge_jitter_frames_p95: Limit {
                    minimum: 2.0,
                    stretch: 1.0,
                },
                polarity_confidence: Limit {
                    minimum: 0.95,
                    stretch: 0.99,
                },
                route_flip_glitches: 0,
            },
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn status_strings_match_pm_contract() {
        assert_eq!(Status::Pass.as_str(), "PASS");
        assert_eq!(Status::Fail.as_str(), "FAIL");
        assert_eq!(Status::NotReady.as_str(), "NOT_READY");
        assert_eq!(Status::BlockedLockBusy.as_str(), "BLOCKED_LOCK_BUSY");
        assert_eq!(
            Status::BlockedPhysicalCapture.as_str(),
            "BLOCKED_PHYSICAL_CAPTURE"
        );
        assert_eq!(Status::BlockedDirtyRoute.as_str(), "BLOCKED_DIRTY_ROUTE");
        assert_eq!(
            Status::BlockedUsbEnumeration.as_str(),
            "BLOCKED_USB_ENUMERATION"
        );
        assert_eq!(
            Status::BlockedIrigUnstable.as_str(),
            "BLOCKED_IRIG_UNSTABLE"
        );
        assert_eq!(
            Status::BlockedUnvalidatedDvs.as_str(),
            "BLOCKED_UNVALIDATED_DVS"
        );
        assert_eq!(Status::BlockedStaleHash.as_str(), "BLOCKED_STALE_HASH");
        assert_eq!(Status::SkippedBusy.as_str(), "SKIPPED_BUSY");
    }

    #[test]
    fn default_thresholds_encode_current_pm_targets() {
        let thresholds = PmSuccessThresholds::default();
        assert_eq!(thresholds.internal.driver_p95_cpu_pct.minimum, 12.0);
        assert_eq!(thresholds.internal.output_read_ratio.stretch, 0.995);
        assert_eq!(thresholds.tone.strongest_sideband_db.minimum, -43.0);
        assert_eq!(thresholds.music.alignment.minimum, 0.970);
        assert_eq!(thresholds.timecode.route_flip_glitches, 0);
    }
}
