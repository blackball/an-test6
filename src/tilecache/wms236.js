/*
 * Call generic wms service for GoogleMaps v2
 * John Deck, UC Berkeley
 * Inspiration & Code from:
 *      Mike Williams http://www.econym.demon.co.uk/googlemaps2/ V2 Reference & custommap code
 *      Brian Flood http://www.spatialdatalogic.com/cs/blogs/brian_flood/archive/2005/07/11/39.aspx V1 WMS code
 *      Kyle Mulka http://blog.kylemulka.com/?p=287  V1 WMS code modifications
 *      http://search.cpan.org/src/RRWO/GPS-Lowrance-0.31/lib/Geo/Coordinates/MercatorMeters.pm
 * 		Modified by Chris Holmes, TOPP to work by default with GeoServer.
 *		Guilhem Vellut <guilhem.vellut@gmail.com> for more accurate Javascript Mercator Fxn
 *
 * Note this only works with gmaps v2.36 and above.  http://johndeck.blogspot.com
 * has scripts
 * that do the same for older gmaps versions - just change from 54004 to 41001.
 *
 * About:
 * This script provides an implementation of GTileLayer that works with WMS
 * services that provide epsg 41001 (Mercator).  This provides a reasonable
 * accuracy on overlays at most zoom levels.  It switches between Mercator
 * and Lat/Long at the myMercZoomLevel variable, defaulting to MERC_ZOOM_DEFAULT
 * of 5.  It also performs the calculation from a GPoint to the appropriate
 * BBOX to pass the WMS.  The overlays could be more accurate, and if you
 * figure out a way to make them so please contribute information back to
 * http://docs.codehaus.org/display/GEOSDOC/Google+Maps.  There is much
 * information at:
 * http://cfis.savagexi.com/articles/2006/05/03/google-maps-deconstructed
 *
 * Use:
 * This script is used by creating a new GTileLayer, setting the required
 * and any desired optional variables, and setting the functions here to
 * override the appropriate GTileLayer ones.
 *
 * At the very least you will need:
 * var myTileLayer= new GTileLayer(new GCopyrightCollection(""),1,17);
 *     myTileLayer.myBaseURL='http://yourserver.org/wms?'
 *     myTileLayer.myLayers='myLayerName';
 *     myTileLayer=CustomGetTileUrl
 *
 * After that you can override the format (myFormat), the level at
 * which the zoom switches (myMercZoomLevel), and the style (myStyles)
 * - be sure to put one style for each layer (both are separated by
 * commas).  You can also override the Opacity:
 *     myTileLayer.myOpacity=0.69
 *     myTileLayer.getOpacity=customOpacity
 *
 * Then you can overlay on google maps with something like:
 * var layer=[G_SATELLITE_MAP.getTileLayers()[0],tileCountry];
 * var custommap = new GMapType(layer, G_SATELLITE_MAP.getProjection(), "WMS");
 * var map = new GMap(document.getElementById("map"));
 *     map.addMapType(custommap);
 */

var FORMAT_DEFAULT="image/png";
CustomGetTileUrl=function(a,b,c) {
	if (this.myFormat == undefined) {
    	this.myFormat = FORMAT_DEFAULT;
	}
	if (typeof(window['this.myStyles'])=="undefined") this.myStyles="";
	var lULP = new GPoint(a.x*256,(a.y+1)*256);
	var lLRP = new GPoint((a.x+1)*256,a.y*256);
	var lUL = G_NORMAL_MAP.getProjection().fromPixelToLatLng(lULP,b,c);
	var lLR = G_NORMAL_MAP.getProjection().fromPixelToLatLng(lLRP,b,c);
	var lBbox=lUL.x+","+lUL.y+","+lLR.x+","+lLR.y;
	var lURL=this.myBaseURL;
	lURL+="&LAYERS="+this.myLayers;
	//lURL+="&FORMAT="+this.myFormat;
	//lURL+="&BGCOLOR=0xFFFFFF";
	//lURL+="&TRANSPARENT=TRUE";
	lURL+="&BBOX="+lBbox;
	lURL+="&WIDTH=256";
	lURL+="&HEIGHT=256";
	return lURL;
}

function customOpacity() { return this.myOpacity; }
