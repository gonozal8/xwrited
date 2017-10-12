<?xml version="1.0"?>
<xsl:stylesheet
  version="1.0"
  xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
  xmlns:db="http://docbook.org/ns/docbook"
  xmlns="http://docbook.org/ns/docbook"
  exclude-result-prefixes="xsl db">

  <xsl:param name="package" select="''" />
  <xsl:param name="version" select="''" />

  <xsl:template match="db:refmeta/db:refmiscinfo[@class = 'source' or
    @class = 'version']"/>

  <xsl:template match="db:refmeta">
    <xsl:copy>
      <xsl:apply-templates/>
      <refmiscinfo class="source"><xsl:value-of select="$package"/></refmiscinfo>
      <refmiscinfo class="version"><xsl:value-of select="$version"/></refmiscinfo>
    </xsl:copy>
  </xsl:template>

  <xsl:template match="@*|node()">
    <xsl:copy>
      <xsl:apply-templates select="@*|node()"/>
    </xsl:copy>
  </xsl:template>

</xsl:stylesheet>
