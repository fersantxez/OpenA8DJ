use crate::topology::{ChannelIndex, Pair, Side, CHANNELS};

#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub struct Route {
    pub source: Option<ChannelIndex>,
    pub inverted: bool,
}

impl Route {
    pub const fn passthrough(channel: ChannelIndex) -> Self {
        Self {
            source: Some(channel),
            inverted: false,
        }
    }

    pub const fn muted() -> Self {
        Self {
            source: None,
            inverted: false,
        }
    }
}

#[derive(Clone, Debug, Eq, PartialEq)]
pub struct RoutingMatrix {
    routes: [Route; CHANNELS],
}

#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub enum RoutingError {
    InvalidSource,
}

impl RoutingMatrix {
    pub fn identity() -> Self {
        let mut routes = [Route::muted(); CHANNELS];
        for (index, route) in routes.iter_mut().enumerate() {
            *route = Route::passthrough(ChannelIndex::new(index).expect("valid channel"));
        }
        Self { routes }
    }

    pub fn dvs_default() -> Self {
        Self::identity()
    }

    pub const fn muted() -> Self {
        Self {
            routes: [Route::muted(); CHANNELS],
        }
    }

    pub fn route(&self, destination: ChannelIndex) -> Route {
        self.routes[destination.get()]
    }

    pub fn set_channel(
        &mut self,
        destination: ChannelIndex,
        source: Option<ChannelIndex>,
    ) -> Result<(), RoutingError> {
        self.routes[destination.get()] = Route {
            source,
            inverted: false,
        };
        Ok(())
    }

    pub fn mute_channel(&mut self, destination: ChannelIndex) {
        self.routes[destination.get()] = Route::muted();
    }

    pub fn invert_channel(&mut self, destination: ChannelIndex, inverted: bool) {
        self.routes[destination.get()].inverted = inverted;
    }

    pub fn map_pair(&mut self, destination: Pair, source: Pair) {
        for side in [Side::Left, Side::Right] {
            self.routes[destination.channel(side).get()] = Route::passthrough(source.channel(side));
        }
    }

    pub fn mute_pair(&mut self, destination: Pair) {
        for side in [Side::Left, Side::Right] {
            self.mute_channel(destination.channel(side));
        }
    }

    pub fn swap_pair_sides(&mut self, pair: Pair) {
        let left = pair.channel(Side::Left);
        let right = pair.channel(Side::Right);
        self.routes.swap(left.get(), right.get());
    }

    pub fn apply_frame(&self, input: &[f32; CHANNELS]) -> [f32; CHANNELS] {
        let mut output = [0.0; CHANNELS];
        for (destination, route) in self.routes.iter().copied().enumerate() {
            let Some(source) = route.source else {
                continue;
            };
            let value = input[source.get()];
            output[destination] = if route.inverted { -value } else { value };
        }
        output
    }
}

impl Default for RoutingMatrix {
    fn default() -> Self {
        Self::identity()
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn identity_preserves_all_channels() {
        let input = [0.1, -0.2, 0.3, -0.4, 0.5, -0.6, 0.7, -0.8];
        assert_eq!(RoutingMatrix::identity().apply_frame(&input), input);
    }

    #[test]
    fn pair_remap_mute_swap_and_invert_are_explicit() {
        let input = [1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0, 8.0];
        let mut routes = RoutingMatrix::identity();
        routes.map_pair(Pair::A, Pair::D);
        routes.mute_pair(Pair::B);
        routes.swap_pair_sides(Pair::C);
        routes.invert_channel(Pair::D.channel(Side::Right), true);

        assert_eq!(
            routes.apply_frame(&input),
            [7.0, 8.0, 0.0, 0.0, 6.0, 5.0, 7.0, -8.0]
        );
    }

    #[test]
    fn dvs_profile_starts_as_identity() {
        assert_eq!(RoutingMatrix::dvs_default(), RoutingMatrix::identity());
    }
}
