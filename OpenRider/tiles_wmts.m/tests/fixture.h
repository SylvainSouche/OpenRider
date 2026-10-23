// A small WMTS capabilities document shaped like the providers' own:
// layer "topo" served REST in UTM 33N and Web Mercator (Kartverket's
// pattern), layers "scan" and "scan.l93" served KVP, the latter in
// IGNF:LAMB93 with limits (IGN's pattern), and a northing-first system.
#pragma once

inline const char *kCapabilities = R"(<?xml version="1.0" encoding="UTF-8"?>
<Capabilities xmlns="http://www.opengis.net/wmts/1.0" xmlns:ows="http://www.opengis.net/ows/1.1"
              xmlns:xlink="http://www.w3.org/1999/xlink" version="1.0.0">
  <!-- comment -->
  <ows:OperationsMetadata>
    <ows:Operation name="GetTile">
      <ows:DCP><ows:HTTP><ows:Get xlink:href="https://example.org/wmts?">
        <ows:Constraint name="GetEncoding"><ows:AllowedValues><ows:Value>KVP</ows:Value></ows:AllowedValues></ows:Constraint>
      </ows:Get></ows:HTTP></ows:DCP>
    </ows:Operation>
  </ows:OperationsMetadata>
  <Contents>
    <Layer>
      <ows:Title>Topo</ows:Title>
      <ows:Identifier>topo</ows:Identifier>
      <Style isDefault="true"><ows:Identifier>default</ows:Identifier></Style>
      <Format>image/png</Format>
      <TileMatrixSetLink><TileMatrixSet>utm33n</TileMatrixSet></TileMatrixSetLink>
      <TileMatrixSetLink><TileMatrixSet>webmercator</TileMatrixSet></TileMatrixSetLink>
      <ResourceURL format="image/png" resourceType="tile"
        template="https://example.org/v1/topo/{Style}/{TileMatrixSet}/{TileMatrix}/{TileRow}/{TileCol}.png"/>
    </Layer>
    <Layer>
      <ows:Identifier>scan</ows:Identifier>
      <Style isDefault="true"><ows:Identifier>normal</ows:Identifier></Style>
      <Format>image/jpeg</Format>
      <TileMatrixSetLink><TileMatrixSet>webmercator</TileMatrixSet></TileMatrixSetLink>
    </Layer>
    <Layer>
      <ows:Identifier>scan.l93</ows:Identifier>
      <Style isDefault="true"><ows:Identifier>normal</ows:Identifier></Style>
      <Format>image/jpeg</Format>
      <TileMatrixSetLink>
        <TileMatrixSet>LAMB93</TileMatrixSet>
        <TileMatrixSetLimits>
          <TileMatrixLimits><TileMatrix>1</TileMatrix><MinTileRow>2</MinTileRow><MaxTileRow>5</MaxTileRow><MinTileCol>1</MinTileCol><MaxTileCol>4</MaxTileCol></TileMatrixLimits>
        </TileMatrixSetLimits>
      </TileMatrixSetLink>
    </Layer>
    <Layer>
      <ows:Identifier>laea</ows:Identifier>
      <Style isDefault="true"><ows:Identifier>default</ows:Identifier></Style>
      <Format>image/png</Format>
      <TileMatrixSetLink><TileMatrixSet>europe</TileMatrixSet></TileMatrixSetLink>
    </Layer>
    <TileMatrixSet>
      <ows:Identifier>utm33n</ows:Identifier>
      <ows:SupportedCRS>urn:ogc:def:crs:EPSG::25833</ows:SupportedCRS>
      <TileMatrix><ows:Identifier>01</ows:Identifier><ScaleDenominator>38685714.285714285</ScaleDenominator>
        <TopLeftCorner>-2500000.0 9045984.0</TopLeftCorner><TileWidth>256</TileWidth><TileHeight>256</TileHeight>
        <MatrixWidth>2</MatrixWidth><MatrixHeight>2</MatrixHeight></TileMatrix>
      <TileMatrix><ows:Identifier>00</ows:Identifier><ScaleDenominator>77371428.57142857</ScaleDenominator>
        <TopLeftCorner>-2500000.0 9045984.0</TopLeftCorner><TileWidth>256</TileWidth><TileHeight>256</TileHeight>
        <MatrixWidth>1</MatrixWidth><MatrixHeight>1</MatrixHeight></TileMatrix>
    </TileMatrixSet>
    <TileMatrixSet>
      <ows:Identifier>webmercator</ows:Identifier>
      <ows:SupportedCRS>urn:ogc:def:crs:EPSG:6.18:3:3857</ows:SupportedCRS>
      <TileMatrix><ows:Identifier>0</ows:Identifier><ScaleDenominator>559082264.0287176</ScaleDenominator>
        <TopLeftCorner>-20037508.342789244 20037508.342789244</TopLeftCorner><TileWidth>256</TileWidth><TileHeight>256</TileHeight>
        <MatrixWidth>1</MatrixWidth><MatrixHeight>1</MatrixHeight></TileMatrix>
    </TileMatrixSet>
    <TileMatrixSet>
      <ows:Identifier>LAMB93</ows:Identifier>
      <ows:SupportedCRS>IGNF:LAMB93</ows:SupportedCRS>
      <TileMatrix><ows:Identifier>0</ows:Identifier><ScaleDenominator>2000000</ScaleDenominator>
        <TopLeftCorner>0 12000000</TopLeftCorner><TileWidth>256</TileWidth><TileHeight>256</TileHeight>
        <MatrixWidth>4</MatrixWidth><MatrixHeight>4</MatrixHeight></TileMatrix>
      <TileMatrix><ows:Identifier>1</ows:Identifier><ScaleDenominator>1000000</ScaleDenominator>
        <TopLeftCorner>0 12000000</TopLeftCorner><TileWidth>256</TileWidth><TileHeight>256</TileHeight>
        <MatrixWidth>8</MatrixWidth><MatrixHeight>8</MatrixHeight></TileMatrix>
    </TileMatrixSet>
    <TileMatrixSet>
      <ows:Identifier>europe</ows:Identifier>
      <ows:SupportedCRS>http://www.opengis.net/def/crs/EPSG/0/3035</ows:SupportedCRS>
      <TileMatrix><ows:Identifier>0</ows:Identifier><ScaleDenominator>10000000</ScaleDenominator>
        <TopLeftCorner>5500000 1500000</TopLeftCorner><TileWidth>256</TileWidth><TileHeight>256</TileHeight>
        <MatrixWidth>8</MatrixWidth><MatrixHeight>8</MatrixHeight></TileMatrix>
    </TileMatrixSet>
  </Contents>
</Capabilities>
)";
