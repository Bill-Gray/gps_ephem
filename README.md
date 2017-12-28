# GPS/GLONASS (GNSS) ephemerides for astronomers

Astronomers occasionally run into the problem of ensuring that the
date/time for their images is correct,  sometimes to within milliseconds.
Verifying that you've gotten the timing right even to within seconds can
be difficult.  This is particularly an issue for people measuring the
positions of fast-moving asteroids.

One solution is to observe GPS/GLONASS (GNSS:  'global navigation
satellite systems') satellites and measure their positions.  The
"correct" positions of these satellites are known to within centimeters,
and they appear to move about 40 arcseconds every second.  If you're able
to measure their positions within your images to an accuracy of one
arcsecond,  you can know your timing to about 1/40 second = 25
milliseconds.

The code in this repository includes a utility which,  given a date/time
and a position on the earth,  will tell you which GPS satellites can be
seen from that point at that time.  It also includes a program you can
then use to generate an ephemeris for a specific GPS satellite;  you can
then go observe that satellite and check to see if the position you
measure matches the "expected" position to your desired level of accuracy.

Note that these utilities are available on-line at

https://projectpluto.com/gps_find.htm

I expect almost everyone will simply visit the above site,  rather than go to
the trouble of building and running the code on their own machines.

#### To be done

* A utility which reads astrometry for a GNSS satellite,  in the Minor Planet
Center,  and tells you how far off your timing was.  Given multiple observations,
it could give the average timing error and its standard deviation.  Again,
this would be provided as an on-line service.

* A utility which can read in a list of observed fields (RA/dec rectangles
and the times they were exposed) and tell you in which fields GNSS
satellites would have been captured.  Such fields would be rare (you
don't often catch a GNSS satellite unintentionally),  but if you've
done enough observing,  you might have a few images with which to check
how good your timing was back then.
