# GPS/GLONASS (GNSS) ephemerides for astronomers

[Click here for further explanation and for on-line versions of these utilities.](https://projectpluto.com/gps_expl.htm)

I expect almost everyone will simply visit the above link,  rather than go to the trouble of building and running the code on their own machines.  In fact, I don't really know if _anyone_ except me has actually built and run these utilities.  (They could,  and you could.  But the on-line utilities are pretty easy to use.)

Astronomers occasionally run into the problem of ensuring that the date/time for their images is correct,  sometimes to within milliseconds. Verifying that you've gotten the timing right even to within seconds can be difficult.  This is particularly an issue for people measuring the positions of fast-moving asteroids.

One solution is to observe GPS/GLONASS (more generally,  GNSS:  'global navigation satellite systems',  including also the European Galileo, Chinese BeiDou, and Japanese QZSS) satellites and measure their positions. The "correct" positions of these satellites are known to within centimeters, and they appear to move about 40 arcseconds every second.  If you're able to measure their positions within your images to an accuracy of one arcsecond,  you can know your timing to about 1/40 second = 25 milliseconds.

Compile the code in this repository (and two dependencies mentioned at bottom), and you'll have :

* A utility which,  given a date/time and a position on the earth, will tell you which GNSS satellites can be seen from that point at that time.

* A utility you can then run to generate an ephemeris for a specific GNSS satellite.  You can then go observe that satellite and check to see if the position you measure matches the "expected" position to your desired level of accuracy.

* A utility which reads astrometry for a GNSS satellite,  in the Minor Planet Center format,  and tells you how far off your timing was.  Given multiple observations, it gives the average timing error and its standard deviation.

* A utility which reads in a list of observed fields (RA/dec rectangles and the times they were exposed) and tells you in which fields GNSS satellites would have been captured.  Such fields are rare (at present, there's roughly one GNSS satellite in every 500 square degrees of sky). But if you've done enough observing,  you might have a few images with which to check how good your timing was back then.  If the errors in that timing prove to be suitably consistent,  you can even correct for timing errors you didn't know about when you gathered the images.

* The code for the on-line versions of these utilities.  I've not gotten around to documenting that as thoroughly as I should.  To do it properly, one needs `cron` jobs on the server to update the Earth orientation parameter files,  the list of observatories,  and so forth.  If you'd like to set up an on-line version of these tools,  please let me know,  and I'll send details.

The various utilities can be compiled for DOS/Windows and for Linux,  and (almost certainly) *BSD and other platforms.  But at present,  the `makefile` is for Linux builds and cross-compiling to Windows with `mingw-64`.

These utilities rely on the following two repositories :

- [`sat_code`](https://github.com/Bill-Gray/sat_code) (code for Earth-orbiting satellite ephemerides)
- [`lunar`](https://github.com/Bill-Gray/lunar) (basic astronomical ephemeris/time functions)
