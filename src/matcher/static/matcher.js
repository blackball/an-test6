// Copyright 2010 Keir Mierle.
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.
//
// Simple manual correspondence selector.

$.extend($.fn.disableTextSelect = function() {
  return this.each(function() {
    if ($.browser.mozilla) {
      $(this).css('MozUserSelect', 'none');
    } else if ($.browser.msie) {
      $(this).bind('selectstart', function() { return false; });
    } else {
      $(this).mousedown(function() { return false; });
    }
  });
});

// XXX Disable this on non-firefox non-chrome browsers.
var log = function(x) {
  console.log(x);
}

var STATE_NOT_LOADED = 1;
var STATE_READY = 2;
var STATE_IMAGE_SELECTED = 3;
var STATE_IMAGE_AND_CATALOG_SELECTED = 4;
var STATE_SAVE_PENDING = 5;

// Start globals

var state = STATE_NOT_LOADED;

// Currently active tweak.
var tweak;

var selectedCatalog = null;
var selectedImage = null;

var ctx;

var canvasPixelsPerImagePixels = 1;

var field = null;

// End globals

function moveToState(newState) {
  oldState = state;
  state = newState;
  log('State transition: ' + oldState + ' to ' + newState);
  $("#save_match").attr("disabled", "disabled");
  $("#remove_match").attr("disabled", "disabled");
  $("#cancel").attr("disabled", "disabled");
  $("#refit").attr("disabled", "disabled");
  if (state == STATE_NOT_LOADED || state == STATE_READY) {
    selectedCatalog = null;
    selectedImage = null;
  }
  if (state == STATE_NOT_LOADED) {
    field = null;
    return;
  }
  $("#refit").removeAttr("disabled");
  if (state == STATE_IMAGE_SELECTED) {
    $("#cancel").removeAttr("disabled");
  }
  if (state == STATE_IMAGE_AND_CATALOG_SELECTED) {
    $("#save_match").removeAttr("disabled");
    $("#remove_match").removeAttr("disabled");
    $("#cancel").removeAttr("disabled");
  }
  draw();
}

function closest(objects, x, y) {
  var closestObject;
  var closestDistance2 = 1000000000;
  var maxDist2 = 100;
  for (var i = 0; i < objects.length; ++i) {
    var dx = objects[i].x - x;
    var dy = objects[i].y - y;
    var d2 = dx*dx + dy*dy;
    if (d2 < closestDistance2) {
      closestObject = objects[i];
      closestDistance2 = d2;
    }
  }
  return ((closestDistance2 < maxDist2) ?
          closestObject : null);
}

function refit() {
  moveToState(STATE_NOT_LOADED);
  log('Fitting....');
  $.post('/solve', { "tweak_id" : tweak.tweak_id,
                     "order"    : $('#order').attr('value')
                   }, function() {
    log('Fitted.');
    loadTweak(tweak.tweak_id);
  });
}

// Set up the event handlers; called on document load.
function matcherStart() {
  $('#reload').click(function() {
    loadTweak($('#tweak_id').attr('value'));
  });
  $('#save_match').click(function() {
    if (state == STATE_IMAGE_AND_CATALOG_SELECTED) {
      saveMatch();
    }
  });
  $('#remove_match').click(function() {
    if (state == STATE_IMAGE_AND_CATALOG_SELECTED) {
      removeMatch();
    }
  });
  $('#cancel').click(function() {
    moveToState(STATE_READY);
  });
  $('#refit').click(function() {
    refit();
  });
  $('#order').keypress(function(e) {
    log('Got keypress for order change.');
    if (e.keyCode == 13 /* Enter */) {
      refit();
    }
  });
  ctx = $('#tweak')[0].getContext('2d');
  $('#tweak_id').keypress(function(e) {
    log('Got keypress for tweak_id loader.');
    if (e.keyCode == 13 /* Enter */) {
      loadTweak($('#tweak_id').attr('value'));
    }
  });
  $('#tweak').click(function(e) {
    var x = (e.pageX - this.offsetLeft) / canvasPixelsPerImagePixels;
    var y = (e.pageY - this.offsetTop)  / canvasPixelsPerImagePixels;
    if (state == STATE_READY) {
      var clickedObject = closest(tweak.image, x, y);
      if (clickedObject != null) {
        // XXX Check if this is already a correspondence, in which case we want
        // to skip to having both selected already (and allow deletion).
        selectedImage = clickedObject;
        moveToState(STATE_IMAGE_SELECTED);
        log('Set new clicked image object: ' + selectedImage.image_id);
      } else {
        log('Click too far from any image objects');
      }
    } else if (state == STATE_IMAGE_SELECTED) {
      var clickedObject = closest(tweak.catalog, x, y);
      if (clickedObject != null) {
        selectedCatalog = clickedObject;
        log('Set new clicked catalog object: ' + selectedCatalog.catalog_id);
        moveToState(STATE_IMAGE_AND_CATALOG_SELECTED);
      } else {
        log('Click too far from any image objects');
      }
    }
  })
  .disableTextSelect();
  moveToState(STATE_NOT_LOADED);
}

function setTweak(newTweak) {
  tweak = newTweak;

  // Make the mapping from image_id to image
  tweak.imageIdToImage = {}
  for (var i = 0; i < tweak.image.length; ++i) {
    tweak.imageIdToImage[tweak.image[i].image_id] = tweak.image[i];
  }

  // Make the mapping from catalog_id to catalog
  tweak.catalogIdToCatalog = {}
  for (var i = 0; i < tweak.catalog.length; ++i) {
    tweak.catalogIdToCatalog[tweak.catalog[i].catalog_id] = tweak.catalog[i];
  }
  
  // Recompute the scale.
  var canvasWidth  = $('#tweak').attr('width');
  var canvasHeight = $('#tweak').attr('height');
  var imageWidth = tweak.width;
  var imageHeight = tweak.height;
  canvasPixelsPerImagePixels = Math.min(canvasWidth / imageWidth,
                                        canvasHeight / imageHeight)
  field = requiredImage(tweak.url);
}

function requiredImage(src) {
  var image = new Image();
  image.src = src;
  image.onload = draw;
  return image;
}

function loadTweak(tweak_id) {
  moveToState(STATE_NOT_LOADED);
  $.getJSON('/tweak', { "tweak_id" : tweak_id }, function(newTweak) {
    log('Got new tweak')
    log('Image points: ' + newTweak.image.length);
    log('Catalog points: ' + newTweak.catalog.length);
    log('Matches: ' + newTweak.matches.length);
    setTweak(newTweak);
    moveToState(STATE_READY);
  });
}

function getCurrentMatch() {
  return { "tweak_id"   : tweak.tweak_id,
           "catalog_id" : selectedCatalog.catalog_id,
           "image_id"   : selectedImage.image_id };
}

// XXX Consider collapsing saveMatch and removeMatch.
function saveMatch() {
  log('Saving...');
  var match = getCurrentMatch();
  selectedCatalog = null;
  selectedImage = null;
  moveToState(STATE_SAVE_PENDING);
  $.post('/savematch', match, function() {
    log('Saved.');
    moveToState(STATE_NOT_LOADED);
    // Reload rather than be clever.
    loadTweak(tweak.tweak_id);
  });
}

function removeMatch() {
  log('Removing match...');
  var match = getCurrentMatch();
  selectedCatalog = null;
  selectedImage = null;
  moveToState(STATE_SAVE_PENDING);
  $.post('/removematch', match, function() {
    log('Removed.');
    moveToState(STATE_NOT_LOADED);
    // Reload rather than be clever.
    loadTweak(tweak.tweak_id);
  });
}

var markerRadius = 5;
var markerWidth = 1;

function drawCross(ctx, x, y, color, thickness) {
  var r = markerRadius;
  ctx.save();
  ctx.lineWidth = thickness ? thickness : markerWidth;
  ctx.strokeStyle = color ? color : '#7755FF';
  ctx.translate(x + 0.5, y + 0.5);
  ctx.beginPath();
  ctx.moveTo(-r,  0);
  ctx.lineTo(+r,  0);
  ctx.moveTo( 0, -r);
  ctx.lineTo( 0, +r);
  ctx.stroke();
  ctx.closePath();
  ctx.restore();
}

function drawCircle(ctx, x, y, color, thickness) {
  var r = markerRadius;
  ctx.save();
  ctx.lineWidth = thickness ? thickness : markerWidth;
  ctx.strokeStyle = color ? color : '#FFCC77';
  ctx.beginPath();
  ctx.arc(x, y, r, 0, 2 * Math.PI, true);
  ctx.closePath();
  ctx.stroke();
  ctx.restore();
}

function draw() {
  if (state == STATE_NOT_LOADED) {
    return;
  }

  ctx.save()

  var canvasWidth  = $('#tweak').attr('width');
  var canvasHeight = $('#tweak').attr('height');
  ctx.clearRect(0, 0, canvasWidth, canvasHeight);

  var imageWidth = tweak.width;
  var imageHeight = tweak.height;

  canvasPixelsPerImagePixels = Math.min(canvasWidth / imageWidth,
                                        canvasHeight / imageHeight)
  ctx.scale(canvasPixelsPerImagePixels, canvasPixelsPerImagePixels)

  // Draw background image, if available.
  if (field && field.complete) {
    ctx.drawImage(field, 0.5, 0.5);
  }

  // Draw image stars
  for (var i = 0; i < tweak.image.length; ++i) {
    drawCircle(ctx, tweak.image[i].x, tweak.image[i].y);
  }

  // Draw catalog stars
  for (var i = 0; i < tweak.catalog.length; ++i) {
    drawCross(ctx, tweak.catalog[i].x, tweak.catalog[i].y);
  }

  // Draw more obvious markers for objects part of matches.
  for (var i = 0; i < tweak.matches.length; ++i) {
    var image = tweak.imageIdToImage[tweak.matches[i].image_id];
    var catalog = tweak.catalogIdToCatalog[tweak.matches[i].catalog_id];
    drawCircle(ctx, image.x, image.y, undefined, 3);
    drawCross(ctx, catalog.x, catalog.y, undefined, 3);
  }

  // Draw lines between corresponding x's and o's.
  ctx.save();
  ctx.beginPath();
  ctx.strokeStyle = '#00FF00';
  ctx.lineWidth = 5;
  for (var i = 0; i < tweak.matches.length; ++i) {
    var image = tweak.imageIdToImage[tweak.matches[i].image_id];
    var catalog = tweak.catalogIdToCatalog[tweak.matches[i].catalog_id];
    ctx.moveTo(image.x, image.y);
    ctx.lineTo(catalog.x, catalog.y);
  }
  ctx.stroke();
  ctx.closePath();
  ctx.restore();

  // Draw the selected x and o, if any.
  if (state == STATE_IMAGE_SELECTED ||
      state == STATE_IMAGE_AND_CATALOG_SELECTED) {
    drawCircle(ctx, selectedImage.x, selectedImage.y, '#FFFFFF', 5);
  }
  if (state == STATE_IMAGE_AND_CATALOG_SELECTED) {
    drawCross(ctx, selectedCatalog.x, selectedCatalog.y, '#FFFFFF', 5);
  }

  ctx.restore()
}
