<?xml version="1.0" encoding="ISO-8859-1"?>
<xsl:stylesheet version="1.0" xmlns:xsl="http://www.w3.org/1999/XSL/Transform">

    <xsl:template match="/">
        <html>
            <head>
                <style type="text/css">
                    body {
                        color : black;
                    }
                    a:hover {
                        text-decoration: underline;
                        background-color : #ffffe0;
                    }
                    a:link {
                        text-decoration: none;
                        color : black;
                    }
                    a:visited {
                        text-decoration: none;
                        color : black;
                    }
                    td {
                        padding-left: 0.5em;
                        padding-right: 0.5em;
                    }
                    .xg-colhead {
                        background-color : #e0f1cd;
                    }
                    .xg-error {
                        color : #ff0000;
                    }
                </style>
            </head>

            <body>

                <h2>Status</h2>

                <table border="1">
                    <tr class="xg-colhead">
                        <th>Info</th>
                        <th>Value</th>
                    </tr>
                    <xsl:if test="/xg-response/xg-status">
                        <tr>
                            <td> XML Gateway Status Code: </td>
                            <xsl:choose>
                                <xsl:when test="/xg-response/xg-status/status-code &gt; 0">
                                    <td class="xg-error"> <xsl:value-of select="/xg-response/xg-status/status-code"/> </td>
                                </xsl:when>
                                <xsl:otherwise>
                                    <td> <xsl:value-of select="/xg-response/xg-status/status-code"/> </td>
                                </xsl:otherwise>
                            </xsl:choose>
                        </tr>
                        <tr>
                            <td> XML Gateway Status Message: </td>
                            <xsl:choose>
                                <xsl:when test="/xg-response/xg-status/status-code &gt; 0">
                                    <td class="xg-error"> <xsl:value-of select="/xg-response/xg-status/status-msg"/> </td>
                                </xsl:when>
                                <xsl:otherwise>
                                    <td> <xsl:value-of select="/xg-response/xg-status/status-msg"/> </td>
                                </xsl:otherwise>
                            </xsl:choose>
                        </tr>
                    </xsl:if>

                    <xsl:if test="/xg-response/*/return-status">
                        <tr>
                            <td> Response Return Code:   </td>
                            <xsl:choose>
                                <xsl:when test="/xg-response/*/return-status/return-code &gt; 0">
                                    <td class="xg-error"> <xsl:value-of select="/xg-response/*/return-status/return-code"/></td>
                                </xsl:when>
                                <xsl:otherwise>
                                    <td> <xsl:value-of select="/xg-response/*/return-status/return-code"/></td>
                                </xsl:otherwise>
                            </xsl:choose>
                        </tr>
                        <tr>
                            <td> Response Return Message:</td>
                            <xsl:choose>
                                <xsl:when test="/xg-response/*/return-status/return-code &gt; 0">
                                    <td class="xg-error"> <xsl:value-of select="/xg-response/*/return-status/return-msg"/></td>
                                </xsl:when>
                                <xsl:otherwise>
                                    <td>  <xsl:value-of select="/xg-response/*/return-status/return-msg"/></td>
                                </xsl:otherwise>
                            </xsl:choose>
                        </tr>
                    </xsl:if>

                    <xsl:if test="/xg-response/*/db-revision-id">
                        <tr>
                            <td> DB Revision:   </td><td>  <xsl:value-of select="/xg-response/*/db-revision-id"/></td>
                        </tr>
                    </xsl:if>

                </table>


                <xsl:if test="/xg-response/*/nodes">

                    <h2>Nodes</h2>
                    <table border="1">
                        <tr class="xg-colhead">
                            <th>Name</th>
                            <th>Type</th>
                            <th>Value</th>

                            <!-- XXXX for now, just hard-code for two attribs if any -->
                            
                            <xsl:if test="/xg-response/*/nodes/node/attribs">
                                <th>Attrib</th><th>Type</th><th>Value</th>
                                <th>Attrib</th><th>Type</th><th>Value</th>
                            </xsl:if>
                        </tr>

                        <xsl:for-each select="/xg-response/*/nodes/node">
                            <tr>
                                <td>
                                    <a style="xg-node-link"><xsl:attribute name="href">/xtree<xsl:value-of select="name"/>/***</xsl:attribute>
                                        <xsl:value-of select="name"/></a></td>
                                <td><xsl:value-of select="type"/></td>
                                <td><xsl:value-of select="value"/></td>
                                <xsl:if test="./attribs">
                                    <xsl:for-each select="./attribs/attrib">
                                        <td><xsl:value-of select="./attribute-id"/></td>
                                        <td><xsl:value-of select="./type"/></td>
                                        <td><xsl:value-of select="./value"/></td>
                                    </xsl:for-each>
                                </xsl:if>

                                <!-- XXXX should dump name parts -->
                            </tr>
                        </xsl:for-each>
                    </table>

                </xsl:if>

                <br/>
                Done.
            </body>
        </html>
    </xsl:template>
</xsl:stylesheet>
