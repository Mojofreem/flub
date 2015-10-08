#/bin/perl
#
# Generates the SDL -> Flub event table. This should only need to be run
# when a relevent change occurs to SDL neccessitating an update.
#
# Usage: perl genevents.pl /path/to/SDL_keycode.h flubEventMapFile.c
#
###############################################################################

%display_names = (	SDLK_UP => {"ANY" => "META_UP"},
					SDLK_DOWN => {"ANY" => "META_DOWN"},
					SDLK_LEFT => {"ANY" => "META_LEFT"},
					SDLK_RIGHT => {"ANY" => "META_RIGHT"},
					SDLK_MENU => {"ANY" => "META_ALTMENU"},
					SDLK_LGUI => {"APPLE" => "META_APPLE", "WIN32" => "META_WINDOWS", "ANY" => "META_WINDOWS"},
					SDLK_RGUI => {"APPLE" => "META_APPLE", "WIN32" => "META_WINDOWS", "ANY" => "META_WINDOWS"} );

@word_breaks = ('minus', 'paren', 'down', 'next', 'up', 'lock', 'bracket', 'brace',
				'separator', 'ampersand', 'again', 'multiply', 'illum', 'toggle',
				'clear', 'entry', 'switch', 'subunit', 'as400', 's400', 'add', 'stop',
				'divide', 'unit', 'vertical', 'bar', 'subtract', 'erase', 'gui',
				'ctrl', 'alt', 'mute', 'dbl', 'recall', 'select', 'prev', 'play',
				'store', 'req', 'sel', 'shift');

@events_mouse = (["FLUB_INPUT_MOUSE_AXIS()", "Mouse Axis", "MOUSE_AXIS", NULL],
				["FLUB_INPUT_MOUSE_XAXIS()", "Mouse X", "MOUSE_X", NULL],
				["FLUB_INPUT_MOUSE_YAXIS()", "Mouse Y", "MOUSE_Y", NULL],
				["FLUB_INPUT_MOUSE_LEFT()", "Mouse Left", "MOUSE_LEFT_BTN", NULL],
				["FLUB_INPUT_MOUSE_MIDDLE()", "Mouse Middle", "MOUSE_MIDDLE_BTN", NULL],
				["FLUB_INPUT_MOUSE_RIGHT()", "Mouse Right", "MOUSE_RIGHT_BTN", NULL],
				["FLUB_INPUT_MOUSE_X1()", "Mouse X1", "MOUSE_X1_BTN", NULL],
				["FLUB_INPUT_MOUSE_X2()", "Mouse X2", "MOUSE_X2_BTN", NULL],
				["FLUB_INPUT_MOUSE_WHEEL_UP()", "WheelUp", "MOUSE_WHEEL_UP", NULL],
				["FLUB_INPUT_MOUSE_WHEEL_DOWN()", "WheelDown", "MOUSE_WHEEL_DOWN", NULL]);


# Return the number of numeric digits in the passed value
sub count_digits($) {
	my $sym = $_[0];
	my $count = 0;

	for(my $k = 0; $k < length($sym); $k++) {
		my $c = substr $sym, $k, 1;
		if((ord($c) >= ord(0)) && (ord($c) <= ord(9))) {
			$count++;
		}
	}
	return $count;
}

# Capitalize the passed in key name value
sub fixup_caps($) {
	my $sym = $_[0];
	my $result = "";

	my $count = count_digits($sym);

	# Keep all F key and single digit key names capitalized
	if((length($sym) <3) || ((length($sym) < 4) && ($count > 1))) {
		return uc($sym);
	}

	$result = uc(substr $sym, 0, 1) . lc(substr $sym, 1);
	my $found = 1;
	while($found) {
		$found = 0;
		foreach(@word_breaks) {
			if($result =~ /$_/) {
				$found = 1;
				$result = $` . uc(substr $&, 0, 1) . (substr $&, 1) . $';
				#' <-- small fix for editor auto formatting choking on the regex post var
				$sym = $result;
			}
		}		
	}

	return $result;
}

# Convert keyname into a proper capitalized form
sub fixup_keyname($) {
	my $sym = $_[0];
	my @result = ();


	@parts = split('_', $sym);
	foreach(@parts) {
		push(@result, fixup_caps($_));
	}
	return join('_', @result);
}

# Preprend "KEY_" and capitalize the passed value
sub gen_bind_name($) {
	my $sym = $_[0];

	return uc("key_" . $sym);
}

# Strip the underscores and properly capitalize the passed value
sub gen_friendly_name($$) {
	my $sym = $_[0];
	my $platform = $_[1];

	return join(' ', split('_', fixup_keyname($sym)));
}

# Get the list of platform special cases for the given key_
sub get_platform_list($) {
	my $sym = $_[0];
	my @result = ();

	if(exists $display_names{$sym}) {
		if(exists $display_names{$sym}{"APPLE"}) {
			push(@result, "APPLE");
		}
		if(exists $display_names{$sym}{"WIN32"}) {
			push(@result, "WIN32");
		}
		if(exists $display_names{$sym}{"ANY"}) {
			push(@result, "ANY");
		}
	}

	if(length(@result) == 0) {
		push(@result, "ANY");
	}

	return @result;
}

# Return the meta name for the symbol passed for the given platform
sub gen_meta_name($$) {
	my $sym = $_[0];
	my $platform = $_[1];

	if(exists $display_names{$sym}) {
		if(exists $display_names{$sym}{$platform}) {
			return '"' . $display_names{$sym}{$platform} . '"';
		}
	}
	return NULL;
}

$g_sym_len = 0;
$g_name_len = 0;
$g_bind_len = 0;
$g_display_len = 0;

sub field_out($$) {
	my $sym = $_[0];
	my $size = $_[1];

	my $result = $sym . (" " x ($size - length($sym)));

	return $result;
}

sub gen_table_entry($$$$) {
	my $sym = $_[0];
	my $name = $_[1];
	my $bind = $_[2];
	my $display = $_[3];

	if(length($sym) > $g_sym_len) {
		$g_sym_len = length($sym) + 1;
	}
	if(length($name) > $g_name_len) {
		$g_name_len = length($name) + 1;
	}
	if(length($bind) > $g_bind_len) {
		$g_bind_len = length($bind) + 1;
	}
	if(length($display) > $g_display_len) {
		$g_display_len = length($display) + 1;
	}

	#if((substr $sym, 0, 5) eq 'SDLK_') {
	#	$sym = 'SDL_SCANCODE_' . uc(substr $sym, 5);
	#}

	return "    {" . field_out($sym . ",", $g_sym_len) . " " . field_out($name . ",", $g_name_len) . " " . field_out($bind . ",", $g_bind_len) . " " . $display . "},\n";
}

sub gen_key_entry($$) {
	my $sym = $_[0];
	my $platform = $_[1];
	my $result = "";
	my $dupe = $sym;

	$bindbase = substr $sym, 5;
	return gen_table_entry($dupe, "\"" . gen_friendly_name($bindbase, $platform) . "\"", "\"" . gen_bind_name($bindbase) . "\"", gen_meta_name($dupe, $platform));
}

sub gen_key_entries($) {
	my $sym = $_[0];
	my @platforms = get_platform_list($sym);
	my %phash = map { $_ => 1 } @platforms;
	my $val = "";

	if(@platforms > 1) {
		if(exists($phash{"APPLE"})) {
			$val .= "#ifdef APPLE\n";
			$val .= gen_key_entry($sym, "APPLE");

			if(exists($phash{"WIN32"}) || exists($phash{"ANY"})) {
				$val .= "#else\n";
			}
		}		

		if(exists($phash{"WIN32"})) {
			$val .= "#ifdef WIN32\n";
			$val .= gen_key_entry($sym, "WIN32");
			if(exists($phash{"ANY"})) {
				$val .= "#else\n";
			}
		}		

		if(exists($phash{"ANY"})) {
			$val .= gen_key_entry($sym, "ANY");
			#$val .= "#endif\n";
		}

		if(exists($phash{"WIN32"})) {
			$val .= "#endif // WIN32\n";
		}		

		if(exists($phash{"APPLE"})) {
			$val .= "#endif // APPLE\n";
		}

		return $val;
	} else {
		return gen_key_entry($sym, "ANY");
	}
}

sub gen_mouse_events() {
	my $result = "";

	foreach(@events_mouse) {
		my $entry = $_;
		$result .= gen_table_entry(@{$entry}[0], '"' . @{$entry}[1] . '"', '"' . @{$entry}[2] . '"', @{$entry}[3]);
	}

	joy#_axis#_X
	joy#_axis#_X
	joy#_btn#
	joy#_ball#_X
	joy#_ball#_y
	joy#_hat#_@



	return $result;
}

sub gen_joy_entry($) {
	my $joy = $_[0];
	my $result = "";

	for(my $axis = 0; $axis < 6; $axis++) {
		$result .= gen_table_entry("FLUB_INPUT_JOY_AXIS(${joy},${axis})",
									"\"Joy${joy} Axis${axis}\"",
									"\"JOY${joy}_AXIS${axis}\"",
									"NULL");
	}
	for(my $button = 0; $button < 16; $button++) {
		$result .= gen_table_entry("FLUB_INPUT_JOY_BTN(${joy},${button})",
									"\"Joy${joy} Btn${button}\"",
									"\"JOY${joy}_BTN${button}\"",
									"NULL");
	}
	for(my $ball = 0; $ball < 4; $ball++) {
		$result .= gen_table_entry("FLUB_INPUT_JOY_BALL(${joy},${ball})",
									"\"Joy${joy} Ball${ball}\"",
									"\"JOY${joy}_BALL${ball}\"",
									"NULL");
	}
	for(my $hat = 0; $hat < 4; $hat++) {
		for(my $idx = 0; $idx < 8; $idx++) {
			my @positions = ('LeftUp', 'Up', 'RightUp', 'Right', 'RightDown', 'Down', 'LeftDown', 'Left');
			my $pos = $positions[$idx];
			$result .= gen_table_entry("FLUB_INPUT_JOY_HAT(${joy},${hat},${idx})",
										"\"Joy${joy} Hat${hat} ${pos}\"",
										"\"JOY${joy}_HAT${hat}_" . uc($pos) . "\"",
										"NULL");
		}
	}

	return $result;
}

sub gen_joy_events() {
	my $result = "";

	for(my $joy = 0; $joy < 8; $joy++) {
		$result .= gen_joy_entry($joy);
	}

	return $result;
}

sub gen_gpad_entry($) {
	my $gpad = $_[0];
	my $result = "";

	for(my $axis = 0; $axis < 6; $axis++) {
		$result .= gen_table_entry("FLUB_INPUT_GPAD_AXIS(${gpad},${axis})",
									"\"Gpad${gpad} Axis${axis}\"",
									"\"GPAD${gpad}_AXIS${axis}\"",
									"NULL");
	}
	for(my $button = 0; $button < 15; $button++) {
		$result .= gen_table_entry("FLUB_INPUT_GPAD_BTN(${gpad},${button})",
									"\"Gpad${gpad} Btn${button}\"",
									"\"GPAD${gpad}_BTN${button}\"",
									"NULL");
	}
	return $result;
}

sub gen_gpad_events() {
	my $result = "";

	for(my $gpad = 0; $gpad < 10; $gpad++) {
		$result .= gen_gpad_entry($gpad);
	}

	return $result;
}

$num_args = $#ARGV + 1;
if($num_args != 2) {
	print "Args: ${num_args}\n";
	print "Usage: genevents.pl /path/to.SDL_keycode.h flubEventTableFile.c\n";
	exit;
}

$sdl_header = $ARGV[0];
$out_file = $ARGV[1];

open(SDLKEYS, $sdl_header) || die "ERROR: Unable to open the SDL header file \"${sdl_header}\": $!";
open(FLUBOUT, '>', $out_file) || die "ERROR: Unable to open the output file \"${out_file}\": $!";

$fh = '';

%sdlkey = ();

while(defined($line = <SDLKEYS>)) {
	if($line =~ /^\s*(SDLK_[^=\s]+).*$/) {
		$sdlkey{$1} = substr $1, 5;
	}
}

close(SDLKEYS);


print FLUBOUT <<'END_CODE_PREAMBLE';
#define FLUB_INPUT_MOUSE_BASE(x)		(300+(x))
#define FLUB_INPUT_MOUSE_AXIS()			FLUB_INPUT_MOUSE_BASE(0)
#define FLUB_INPUT_MOUSE_XAXIS()		FLUB_INPUT_MOUSE_BASE(1)
#define FLUB_INPUT_MOUSE_YAXIS()		FLUB_INPUT_MOUSE_BASE(2)
#define FLUB_INPUT_MOUSE_LEFT()			FLUB_INPUT_MOUSE_BASE(3)
#define FLUB_INPUT_MOUSE_MIDDLE()		FLUB_INPUT_MOUSE_BASE(4)
#define FLUB_INPUT_MOUSE_RIGHT()		FLUB_INPUT_MOUSE_BASE(5)
#define FLUB_INPUT_MOUSE_X1()			FLUB_INPUT_MOUSE_BASE(6)
#define FLUB_INPUT_MOUSE_X2()			FLUB_INPUT_MOUSE_BASE(7)
#define FLUB_INPUT_MOUSE_BTN(x)			FLUB_INPUT_MOUSE_BASE((x)+3)
#define FLUB_INPUT_MOUSE_WHEEL_UP()		FLUB_INPUT_MOUSE_BASE(8)
#define FLUB_INPUT_MOUSE_WHEEL_DOWN()	FLUB_INPUT_MOUSE_BASE(9)

#define FLUB_INPUT_JOY_BASE(x)			(310+(x))
#define FLUB_INPUT_JOY_DEV(j)			(FLUB_INPUT_JOY_BASE((j)*62))
#define FLUB_INPUT_JOY_AXIS(j,a)		(FLUB_INPUT_JOY_DEV(j)+(a))
#define FLUB_INPUT_JOY_BTN(j,b)			(FLUB_INPUT_JOY_DEV(j)+6+(b))
#define FLUB_INPUT_JOY_BALL(j,b)		(FLUB_INPUT_JOY_DEV(j)+22+((b)*2))
#define FLUB_INPUT_JOY_HAT(j,h,p)		(FLUB_INPUT_JOY_DEV(j)+30+((h)*8)+(p))

#define FLUB_INPUT_GPAD_BASE(x)			(806+(x))
#define FLUB_INPUT_GPAD_DEV(g)			(FLUB_INPUT_GPAD_BASE((g)*21))
#define FLUB_INPUT_GPAD_AXIS(g,a)		(FLUB_INPUT_GPAD_DEV(g)+(a))
#define FLUB_INPUT_GPAD_BTN(g,b)		(FLUB_INPUT_GPAD_DEV((g)+6+(b)))

typedef struct flubInputEventEntry_s {
	int index;
	const char *name;
	const char *bindname;
	const char *displayName;
} flubInputEventEntry_t;

static flubInputEventEntry_t _flubInputEventTable[] = {
END_CODE_PREAMBLE

# First run, calculate field lengths
while(($skey, $sval) = each(%sdlkey)) {
	gen_key_entries($skey);
}
gen_mouse_events();
gen_joy_events();
gen_gpad_events();

# Generate the event table file
while(($skey, $sval) = each(%sdlkey)) {
	print FLUBOUT gen_key_entries($skey);
}
print FLUBOUT gen_mouse_events();
print FLUBOUT gen_joy_events();
print FLUBOUT gen_gpad_events();

print FLUBOUT gen_table_entry("0", "NULL", "NULL", "NULL");
print FLUBOUT "};\n\n";

close(FLUBOUT);
