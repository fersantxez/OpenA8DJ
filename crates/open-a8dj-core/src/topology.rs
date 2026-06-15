#![allow(clippy::missing_const_for_fn)]

pub const PAIRS: usize = 4;
pub const SIDES_PER_PAIR: usize = 2;
pub const CHANNELS: usize = PAIRS * SIDES_PER_PAIR;

pub const ALL_PAIRS: [Pair; PAIRS] = [Pair::A, Pair::B, Pair::C, Pair::D];
pub const ALL_SIDES: [Side; SIDES_PER_PAIR] = [Side::Left, Side::Right];

#[derive(Clone, Copy, Debug, Eq, Hash, Ord, PartialEq, PartialOrd)]
#[repr(u8)]
pub enum Pair {
    A = 0,
    B = 1,
    C = 2,
    D = 3,
}

impl Pair {
    pub const fn from_index(index: usize) -> Option<Self> {
        match index {
            0 => Some(Self::A),
            1 => Some(Self::B),
            2 => Some(Self::C),
            3 => Some(Self::D),
            _ => None,
        }
    }

    pub const fn index(self) -> usize {
        self as usize
    }

    pub const fn name(self) -> &'static str {
        match self {
            Self::A => "A",
            Self::B => "B",
            Self::C => "C",
            Self::D => "D",
        }
    }

    pub const fn channel(self, side: Side) -> ChannelIndex {
        ChannelIndex((self.index() * SIDES_PER_PAIR + side.index()) as u8)
    }

    pub const fn channels(self) -> (ChannelIndex, ChannelIndex) {
        (self.channel(Side::Left), self.channel(Side::Right))
    }
}

#[derive(Clone, Copy, Debug, Eq, Hash, Ord, PartialEq, PartialOrd)]
#[repr(u8)]
pub enum Side {
    Left = 0,
    Right = 1,
}

impl Side {
    pub const fn from_index(index: usize) -> Option<Self> {
        match index {
            0 => Some(Self::Left),
            1 => Some(Self::Right),
            _ => None,
        }
    }

    pub const fn index(self) -> usize {
        self as usize
    }

    pub const fn name(self) -> &'static str {
        match self {
            Self::Left => "left",
            Self::Right => "right",
        }
    }

    pub const fn opposite(self) -> Self {
        match self {
            Self::Left => Self::Right,
            Self::Right => Self::Left,
        }
    }
}

#[derive(Clone, Copy, Debug, Eq, Hash, Ord, PartialEq, PartialOrd)]
pub struct ChannelIndex(u8);

impl ChannelIndex {
    pub const fn new(index: usize) -> Option<Self> {
        if index < CHANNELS {
            Some(Self(index as u8))
        } else {
            None
        }
    }

    pub const fn get(self) -> usize {
        self.0 as usize
    }

    pub const fn pair(self) -> Pair {
        match self.get() / SIDES_PER_PAIR {
            0 => Pair::A,
            1 => Pair::B,
            2 => Pair::C,
            _ => Pair::D,
        }
    }

    pub const fn side(self) -> Side {
        match self.get() % SIDES_PER_PAIR {
            0 => Side::Left,
            _ => Side::Right,
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn pair_channels_are_interleaved_stereo_streams() {
        assert_eq!(Pair::A.channels(), (ChannelIndex(0), ChannelIndex(1)));
        assert_eq!(Pair::B.channels(), (ChannelIndex(2), ChannelIndex(3)));
        assert_eq!(Pair::C.channels(), (ChannelIndex(4), ChannelIndex(5)));
        assert_eq!(Pair::D.channels(), (ChannelIndex(6), ChannelIndex(7)));
    }

    #[test]
    fn channel_index_round_trips_pair_and_side() {
        for pair in ALL_PAIRS {
            for side in ALL_SIDES {
                let channel = pair.channel(side);
                assert_eq!(channel.pair(), pair);
                assert_eq!(channel.side(), side);
            }
        }
        assert_eq!(ChannelIndex::new(CHANNELS), None);
    }
}
